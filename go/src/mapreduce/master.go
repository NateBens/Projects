package mapreduce

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net"
	"net/http"
	"net/rpc"
	"os"
	"strings"
	"sync"
	"time"
)

//Start : start function
func Start(client Interface) error {
	var isMaster bool
	flag.BoolVar(&isMaster, "master", false, "Boolean indicating whether instance is the master (default false/worker)")
	var port string
	flag.StringVar(&port, "port", "3410", "Port to listen on")
	var masterAddress string
	flag.StringVar(&masterAddress, "maddress", getLocalAddress()+":3410", "Address of master, ignored if master")
	var doSplit bool
	//flag.BoolVar(&doSplit, "split", true, "Boolean indicating whether to split input database or not, ignored if worker")
	doSplit = true
	var M, R int
	flag.IntVar(&M, "m", 9, "Number of Map Tasks, ignored if worker")
	flag.IntVar(&R, "r", 3, "Number of Reduce Tasks, ignored if worker")
	var infile string
	flag.StringVar(&infile, "infile", "austen.sqlite3", "input database file, ignored if worker.")
	flag.Parse()
	var err error

	if !isMaster {
		address := getAddress(port)
		log.Printf("Worker Listening on address: %v", address)
		log.Printf("Workers master located at address: %v", masterAddress)
		err = worker(address, masterAddress, client)
	} else {
		address := getAddress(port)
		log.Printf("Master Listening on address: %v", address)
		err = master(address, infile, M, R, doSplit)
	}
	log.Printf("Goodbye!")
	return err
}

func master(address, infile string, m, r int, doSplit bool) error {
	if !doSplit {
		return fmt.Errorf("as the program stands right now you must split the database in order to serve the files formed by the split")
	}
	//Split the input file and start an HTTP server to serve source chunks to map workers.
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
	log.Printf("Splitting input file %v into %v pieces...", infile, m)
	paths, err := splitDatabase(infile, tempdir+"/map_%d_source.sqlite3", m)
	if err != nil {
		log.Printf("%v", paths)
		log.Fatalf("master: %v", err)
	}

	fmt.Println("	->Done")
	log.Printf("Serving source files at http://%v/data/", address)
	//Generate the full set of map tasks and reduce tasks
	log.Printf("Generating %v Map and %v Reduce tasks...", m, r)
	var maptasks []*MapTask
	for i := 0; i < m; i++ {
		maptask := new(MapTask)
		maptask.M = m
		maptask.R = r
		maptask.N = i
		//maptask.SourceHost = fmt.Sprintf("http://%v/data/map_%d_source.sqlite3", address, i)
		maptask.SourceHost = fmt.Sprintf("http://%v/data/map_%d_source.sqlite3", address, i)
		maptasks = append(maptasks, maptask)
	}
	var reducetasks []*ReduceTask
	for i := 0; i < r; i++ {
		reducetask := new(ReduceTask)
		reducetask.M = m
		reducetask.R = r
		reducetask.N = i
		reducetasks = append(reducetasks, reducetask)
	}
	fmt.Println("	->Done")
	//Create and start an RPC server to handle incoming client requests.
	mastertask := new(MasterTask)
	mastertask.M = m
	mastertask.R = r
	mastertask.MapTasks = maptasks
	mastertask.ReduceTasks = reducetasks
	finished := make(chan error)
	mastertask.MFinished = false
	mastertask.RFinished = false
	mastertask.FinishedChan = finished
	log.Printf("Starting RPC Server listening at %v", address)
	/*go func() {
		if err := rpc.Register(mastertask); err != nil {
			log.Fatalf("rpc.Register: %v", err)
		}
		rpc.HandleHTTP()
		l, e := net.Listen("tcp", address)
		if e != nil {
			log.Fatal("listen error:", e)
		}
		if err := http.Serve(l, nil); err != nil {
			log.Fatalf("http.Serve: %v", err)
		}
	}()*/
	go func() {
		if err := rpc.Register(mastertask); err != nil {
			log.Fatalf("rpc.Register: %v", err)
		}
		rpc.HandleHTTP()
		http.Handle("/data/", http.StripPrefix("/data", http.FileServer(http.Dir(tempdir))))
		if err := http.ListenAndServe(address, nil); err != nil {
			log.Printf("Error in HTTP server for %s: %v", address, err)
		}
	}()
	err = <-finished
	outputdbpath := ("outputdatabase.sqlite3")
	log.Printf("All tasks completed, creating Output Database %v...", outputdbpath)
	//log.Printf("mastertask.ReduceOutputs: %v", mastertask.ReduceOuputs)
	_, err = mergeDatabases(mastertask.ReduceOuputs, outputdbpath, tempdir)
	if err != nil {
		return fmt.Errorf("Master: %v", err)
	}
	mastertask.RFinished = true
	fmt.Println("	->Done")
	log.Printf("Shutting master down in 5 seconds...")
	time.Sleep(5000 * time.Millisecond)
	return err
}

// MasterTask : Struct that the master server uses
type MasterTask struct {
	Client                 Interface
	M, R                   int
	IssuedM, IssuedR       int
	CompletedM, CompletedR int
	MapTasks               []*MapTask
	ReduceTasks            []*ReduceTask
	ReduceOuputs           []string
	MFinished, RFinished   bool
	FinishedChan           chan error
	Mutex                  sync.Mutex
}
type Nothing struct{}

type MasterRequest struct {
	Command   string
	Address   string
	Pathnames []string
}
type MasterResponse struct {
	Command    string
	Maptask    *MapTask
	Reducetask *ReduceTask
}

func (mastertask *MasterTask) MasterRequest(request Nothing, response *MasterResponse) error {
	mastertask.Mutex.Lock()
	defer mastertask.Mutex.Unlock()
	log.Printf("Work request received...")
	if !mastertask.MFinished && !mastertask.RFinished {
		//issue map tasks
		if mastertask.IssuedM >= mastertask.M {
			fmt.Println("	->issuing wait command.")
			response.Command = "wait"
		} else {
			fmt.Println("	->issuing map task.")
			response.Command = "map"
			response.Maptask = mastertask.MapTasks[mastertask.IssuedM]
			mastertask.IssuedM++
		}
	} else if mastertask.MFinished && !mastertask.RFinished {
		//issue reduce tasks
		if mastertask.IssuedR >= mastertask.R {
			fmt.Println("	->issuing wait command.")
			response.Command = "wait"
		} else {
			fmt.Println("	->issuing reduce task.")
			response.Command = "reduce"
			response.Reducetask = mastertask.ReduceTasks[mastertask.IssuedR]
			mastertask.IssuedR++
		}
	} else if mastertask.MFinished && mastertask.RFinished {
		// tasks complete
		fmt.Println("	->issuing shutdown command.")
		response.Command = "shutdown"
		//mastertask.FinishedChan <- nil
	} else {
		mastertask.FinishedChan <- fmt.Errorf("MasterRequst Error")
	}
	return nil
}
func (mastertask *MasterTask) MasterResponse(request MasterRequest, response *Nothing) error {
	mastertask.Mutex.Lock()
	defer mastertask.Mutex.Unlock()
	log.Printf("Work response recieved from worker %v...", request.Address)
	if request.Command == "map" {
		mastertask.CompletedM++
		pathstring := fmt.Sprintf("http://%v/data/", request.Address)
		for i := 0; i < mastertask.R; i++ {
			substring := fmt.Sprintf("_%v.sqlite3", i)
			for j := 0; j < len(request.Pathnames); j++ {
				if strings.Contains(request.Pathnames[j], substring) {
					mastertask.ReduceTasks[i].SourceHosts = append(mastertask.ReduceTasks[i].SourceHosts, pathstring+request.Pathnames[j])
				}
			}
		}
		fmt.Printf("	->recieved %v MapOutput paths.\n", len(request.Pathnames))
		if mastertask.CompletedM == mastertask.M {
			mastertask.MFinished = true
			log.Printf("Map Tasks Completed!")
			/*for i := 0; i < len(mastertask.ReduceTasks); i++ {
				log.Printf("%v", mastertask.ReduceTasks[i].SourceHosts)
			}*/

		}
		return nil
	} else if request.Command == "reduce" {
		mastertask.CompletedR++
		pathstring := fmt.Sprintf("http://%v/data/", request.Address)
		if len(request.Pathnames) != 1 {
			mastertask.FinishedChan <- fmt.Errorf("MasterResponse pathnames length Error")
			log.Panicf("MasterResponse pathnames length Error")
		} else {
			mastertask.ReduceOuputs = append(mastertask.ReduceOuputs, pathstring+request.Pathnames[0])
			fmt.Printf("	->recieved ReduceOutput path. %v\n", pathstring+request.Pathnames[0])

		}
		if mastertask.CompletedR == mastertask.R {
			//mastertask.RFinished = true
			log.Printf("Reduce Tasks Completed!")
			mastertask.FinishedChan <- nil
		}
	} else {
		mastertask.FinishedChan <- fmt.Errorf("MasterResponse Error")
	}
	return nil
}

func call(address string, method string, request interface{}, response interface{}) error {
	client, err := rpc.DialHTTP("tcp", address)
	if err != nil {
		//log.Printf("rpc.DialHTTP: %v", err)
		return err
	}
	defer client.Close()

	if err := client.Call(method, request, response); err != nil {
		//log.Printf("client.Call: %s: %v", method, err)
		return err
	}
	return nil
}

func getAddress(port string) string {
	localaddress := getLocalAddress()
	address := (localaddress + ":" + port)
	return address
}

func getLocalAddress() string {
	var localaddress string

	ifaces, err := net.Interfaces()
	if err != nil {
		panic("init: failed to find network interfaces")
	}

	// find the first non-loopback interface with an IP address
	for _, elt := range ifaces {
		if elt.Flags&net.FlagLoopback == 0 && elt.Flags&net.FlagUp != 0 {
			addrs, err := elt.Addrs()
			if err != nil {
				panic("init: failed to get addresses for network interface")
			}

			for _, addr := range addrs {
				if ipnet, ok := addr.(*net.IPNet); ok {
					if ip4 := ipnet.IP.To4(); len(ip4) == net.IPv4len {
						localaddress = ip4.String()
						break
					}
				}
			}
		}
	}
	if localaddress == "" {
		panic("init: failed to find non-loopback interface with valid address on this node")
	}

	return localaddress
}
