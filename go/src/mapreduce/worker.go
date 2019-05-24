package mapreduce

import (
	"database/sql"
	"fmt"
	"hash/fnv"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

// MapTask : MapTask
type MapTask struct {
	M, R       int    // total number of map and reduce tasks
	N          int    // map task number, 0-based
	SourceHost string // address of host with map input file
}

// ReduceTask : ReduceTask
type ReduceTask struct {
	M, R        int      // total number of map and reduce tasks
	N           int      // reduce task number, 0-based
	SourceHosts []string // addresses of map workers
}

// Pair : Pair
type Pair struct {
	Key   string
	Value string
}

// Interface : Interface
type Interface interface {
	Map(key, value string, output chan<- Pair) error
	Reduce(key string, values <-chan string, output chan<- Pair) error
}

func mapSourceFile(m int) string       { return fmt.Sprintf("map_%d_source.sqlite3", m) }
func mapInputFile(m int) string        { return fmt.Sprintf("map_%d_input.sqlite3", m) }
func mapOutputFile(m, r int) string    { return fmt.Sprintf("map_%d_output_%d.sqlite3", m, r) }
func reduceInputFile(r int) string     { return fmt.Sprintf("reduce_%d_input.sqlite3", r) }
func reduceOutputFile(r int) string    { return fmt.Sprintf("reduce_%d_output.sqlite3", r) }
func reducePartialFile(r int) string   { return fmt.Sprintf("reduce_%d_partial.sqlite3", r) }
func reduceTempFile(r int) string      { return fmt.Sprintf("reduce_%d_temp.sqlite3", r) }
func makeURL(host, file string) string { return fmt.Sprintf("http://%s/data/%s", host, file) }

func worker(address, masterAddress string, client Interface) error {
	dir, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("Creating tempdir...")
	tempdir, err := ioutil.TempDir(dir, "data")
	if err != nil {
		log.Fatalf("master: ioutil.TempDir: %v", err)
	}
	log.Printf("Created tempdir %v", tempdir)
	defer os.RemoveAll(tempdir)
	log.Printf("Serving intermediate files at http://%v/data/", address)
	go func() {
		http.Handle("/data/", http.StripPrefix("/data", http.FileServer(http.Dir(tempdir))))
		if err := http.ListenAndServe(address, nil); err != nil {
			log.Printf("Error in HTTP server for %s: %v", address, err)
		}
	}()
	finished := false
	var junk Nothing
	for !finished {
		response := new(MasterResponse)
		log.Printf("Requesting work from Master at %v...", masterAddress)
		if err := call(masterAddress, "MasterTask.MasterRequest", junk, &response); err != nil {
			log.Fatalf("Worker %v MasterRequst call error to master at %v. err: %v", address, masterAddress, err)
		}
		if response.Command == "wait" {
			fmt.Println("	->Wait command recieved, waiting for 3 seconds...")
			time.Sleep(3000 * time.Millisecond)
		} else if response.Command == "map" {
			fmt.Println("	->Map Task recieved!")
			//process the MapTask
			err = response.Maptask.Process(tempdir, client)
			if err != nil {
				return fmt.Errorf("Worker: %v", err)
			}
			//reply to the Master
			request := new(MasterRequest)
			request.Command = "map"
			request.Address = address
			files, err := ioutil.ReadDir(tempdir)
			if err != nil {
				return fmt.Errorf("worker: ioutil.ReadDir, err %v", err)
			}
			for i := 0; i < len(files); i++ {
				matchstring := fmt.Sprintf("%v_output_", response.Maptask.N)
				if strings.Contains(files[i].Name(), matchstring) {
					request.Pathnames = append(request.Pathnames, files[i].Name())
				}
			}
			log.Printf("Replying to master with %v pathnames.", len(request.Pathnames))
			if err := call(masterAddress, "MasterTask.MasterResponse", request, &junk); err != nil {
				return fmt.Errorf("Worker %v MasterResponse call error to master at %v. err: %v", address, masterAddress, err)
			}

		} else if response.Command == "reduce" {
			fmt.Println("	->Reduce Task recieved!")
			//process the ReduceTast
			err = response.Reducetask.Process(tempdir, client)
			if err != nil {
				return fmt.Errorf("Worker: %v", err)
			}
			//reply to the Master
			request := new(MasterRequest)
			request.Command = "reduce"
			request.Address = address
			files, err := ioutil.ReadDir(tempdir)
			if err != nil {
				return fmt.Errorf("worker: ioutil.ReadDir, err %v", err)
			}
			var latestfile string
			for i := 0; i < len(files); i++ {
				if strings.Contains(files[i].Name(), "_output.sqlite3") {
					//request.Pathnames[0] = files[i].Name()
					//request.Pathnames = append(request.Pathnames, files[i].Name())
					//break
					latestfile = files[i].Name()
				}
			}
			request.Pathnames = append(request.Pathnames, latestfile)
			if err := call(masterAddress, "MasterTask.MasterResponse", request, &junk); err != nil {
				return fmt.Errorf("Worker %v MasterResponse call error to master at %v. err: %v", address, masterAddress, err)
			}
		} else if response.Command == "shutdown" {
			fmt.Println("	->Shutdown command recieved...")
			finished = true
		} else {
			return fmt.Errorf("worker received an unknown command from master")
		}
	}
	log.Printf("Worker at %v shutting down", address)
	return err
}

// Process : MapProcess
func (task *MapTask) Process(tempdir string, client Interface) error {
	log.Printf("Process Called on MapTask %v", task.N)
	inputfilepath, err := download(task.SourceHost, tempdir)
	if err != nil {
		return fmt.Errorf("MapTask: Process: %v", err)
	}
	//log.Printf("inputfilepath: %v", inputfilepath)
	inputdb, err := openDatabase(inputfilepath)
	if err != nil {
		return fmt.Errorf("MapTask: Process: %v", err)
	}
	defer inputdb.Close()
	// Create the output files
	var outputDBs []*sql.DB
	for i := 0; i < task.R; i++ {

		outputpath := mapOutputFile(task.N, i)
		outputDB, err := createDatabase(tempdir + "/" + outputpath)
		if err != nil {
			return fmt.Errorf("MapTask: Process: %v", err)
		}
		outputDBs = append(outputDBs, outputDB)
		defer outputDB.Close()
	}
	var statements []*sql.Stmt
	for i := 0; i < task.R; i++ {
		stmt, err := outputDBs[i].Prepare("INSERT INTO pairs (key,value) VALUES (?,?);")
		if err != nil {
			return fmt.Errorf("Maptask: Process: Prepare: %v", err)
		}
		statements = append(statements, stmt)
		defer stmt.Close()
	}

	//Run a database query to select all pairs from the source file
	rows, err := inputdb.Query("SELECT key, value FROM pairs;")
	if err != nil {
		inputdb.Close()
		return fmt.Errorf("Process: db.Query(Select): %v", err)
	}
	defer rows.Close()
	// For each pair:
	processedPairs := 0
	generatedPairs := 0

	for rows.Next() {
		output := make(chan Pair, 100)
		finished := make(chan error)
		var key string
		var value string
		if err := rows.Scan(&key, &value); err != nil {
			inputdb.Close()
			return fmt.Errorf("MapTask: Process: rows.Scan: %v", err)
		}
		//log.Printf("KEY: %v - VALUE: %v", key, value)
		processedPairs++
		//Call client.Map with the data pair
		go func() {
			for pair := range output {
				//var pair Pair
				//pair = <-output
				//log.Printf("KEY: %v - VALUE: %v", pair.Key, pair.Value)
				generatedPairs++
				hash := fnv.New32() // from the stdlib package hash/fnv
				hash.Write([]byte(pair.Key))
				r := int(hash.Sum32()) % task.R
				/*if _, err := outputDBs[r].Exec("INSERT INTO pairs (key,value) VALUES (?,?);", key, value); err != nil {
					err = fmt.Errorf("MapTask: Process: Exec(Insert) %v", err)
				}*/
				if _, err := statements[r].Exec(pair.Key, pair.Value); err != nil {
					err = fmt.Errorf("MapTask: Process: Exec(Insert) %v", err)
				}
			}
			finished <- nil
		}()
		err = client.Map(key, value, output)
		if err != nil {
			return fmt.Errorf("MapTask: Process: Client.Map: %v", err)
		}
		err = <-finished
		/*Gather all Pair objects the client feeds back through the output channel
		and insert each pair into the appropriate output database.
		This process stops when the client closes the channel.*/

	}
	log.Printf("map task processed %v pairs, generated %v pairs", processedPairs, generatedPairs)
	return err
}

//Process : ReduceProcess
func (task *ReduceTask) Process(tempdir string, client Interface) error {
	log.Printf("Process Called on ReduceTask %v", task.N)
	inputfilepath := reduceInputFile(task.N)
	inputdb, err := mergeDatabases(task.SourceHosts, tempdir+"/"+inputfilepath, tempdir)
	if err != nil {
		return fmt.Errorf("ReduceTask: Process: %v", err)
	}
	defer inputdb.Close()

	outputfilepath := reduceOutputFile(task.N)
	outputdb, err := createDatabase(tempdir + "/" + outputfilepath)
	if err != nil {
		return fmt.Errorf("ReduceTask: Process: %v", err)
	}
	defer outputdb.Close()

	rows, err := inputdb.Query("SELECT key, value FROM pairs ORDER BY key, value;")
	if err != nil {
		inputdb.Close()
		return fmt.Errorf("ReduceTask: Process: db.Query(Select): %v", err)
	}
	var previousKey string
	previousKey = ""
	var input chan string
	var output chan Pair
	var finished chan struct{}
	processedPairs := 0
	generatedPairs := 0
	stmt, err := outputdb.Prepare("INSERT INTO pairs (key,value) VALUES (?,?);")
	if err != nil {
		return fmt.Errorf("ReduceTask: Process: Prepare: %v", err)
	}
	defer stmt.Close()
	for rows.Next() {
		var key string
		var value string
		if err := rows.Scan(&key, &value); err != nil {
			inputdb.Close()
			return fmt.Errorf("ReduceTask: Process: rows.Scan: %v", err)
		}
		processedPairs++
		//log.Printf("Key=%v, PrevKey=%v, inputIsNil=%v", key, previousKey, input == nil)
		if key != previousKey && input != nil {
			close(input)
			input = nil
			//log.Printf("Waiting 1")
			<-finished
			//log.Printf("Waiting 2")
			<-finished
			//log.Printf("Waiting 3")
		}
		if key != previousKey {
			previousKey = key
			input = make(chan string, 100)
			output = make(chan Pair, 10)
			finished = make(chan struct{})
			go func(output chan Pair) {
				for pair := range output {
					generatedPairs++
					//log.Printf("pair.Key=%v, pair.Value=%v", pair.Key, pair.Value)
					if _, err := stmt.Exec(pair.Key, pair.Value); err != nil {
						err = fmt.Errorf("ReduceTask: Process: Exec(Insert) %v", err)
					}
				}
				//log.Printf("Exec Finished 1")
				finished <- struct{}{}
				//log.Printf("Exec Finished 2")
			}(output)
			go func(input chan string, output chan Pair) {
				err = client.Reduce(key, input, output)
				if err != nil {
					err = fmt.Errorf("ReduceTask: Process: Client.Reduce: %v", err)
					log.Fatalf("%v", err)
				}
				//log.Printf("Client.Reduce Finished 1")
				finished <- struct{}{}
				//log.Printf("Client.Reduce Finished 2")
			}(input, output)
		}
		input <- value
	}
	close(input)
	input = nil
	<-finished
	<-finished
	log.Printf("reduce task processed %v pairs, generated %v pairs", processedPairs, generatedPairs)
	return err
}
