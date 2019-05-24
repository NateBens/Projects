package mapreduce

import (
	"database/sql"
	"errors"
	"fmt"
	"io"
	"log"
	"math"
	"net/http"
	"net/url"
	"os"
	"strings"

	_ "github.com/mattn/go-sqlite3"
)

func openDatabase(path string) (*sql.DB, error) {
	db, err := sql.Open("sqlite3", path)
	if err != nil {
		return nil, fmt.Errorf("openDatabase: sql.Open: %v", err)
	}
	if _, err := db.Exec("pragma synchronous = off;"); err != nil {
		db.Close()
		return nil, fmt.Errorf("openDatabase: db.Exec(synchronous): %v", err)
	}
	if _, err := db.Exec("pragma journal_mode = off;"); err != nil {
		db.Close()
		return nil, fmt.Errorf("openDatabase: db.Exec(journal_mode): %v", err)
	}
	return db, nil
}

func createDatabase(path string) (*sql.DB, error) {
	var db *sql.DB
	if _, err := os.Stat(path); err == nil {
		// database exists
		//log.Printf("createDatabase called on database that  %v already exists, deleting it.", path)
		os.Remove(path)
		db, err = sql.Open("sqlite3", path)
		if err != nil {
			return nil, fmt.Errorf("createDatabase: sql.Open: %v", err)
		}
	} else if os.IsNotExist(err) {
		// path/to/whatever does *not* exist
		db, err = sql.Open("sqlite3", path)
		if err != nil {
			return nil, fmt.Errorf("createDatabase: sql.Open: %v", err)
		}
	} else {
		// Schrodinger: file may or may not exist. See err for details.
		log.Fatalf("createDatabase %v", err)
	}
	if _, err := db.Exec("pragma synchronous = off;"); err != nil {
		db.Close()
		return nil, fmt.Errorf("createDatabase: db.Exec(synchronous): %v", err)
	}
	if _, err := db.Exec("pragma journal_mode = off;"); err != nil {
		db.Close()
		return nil, fmt.Errorf("createDatabase: db.Exec(journal_mode): %v", err)
	}
	if _, err := db.Exec("create table pairs (key text, value text);"); err != nil {
		db.Close()
		return nil, fmt.Errorf("createDatabase: db.Exec(create table): %v", err)
	}
	return db, nil
}

func splitDatabase(source, outputPattern string, m int) ([]string, error) {
	db, err := openDatabase(source)
	defer db.Close()
	if err != nil {
		return nil, fmt.Errorf("splitDatabase: sql.Open: %v", err)
	}
	pairscount, err := db.Query("SELECT count(1) as count FROM pairs;")
	if err != nil {
		db.Close()
		return nil, fmt.Errorf("splitDatabase: db.Query(count): %v", err)
	}
	var count int
	for pairscount.Next() {
		if err := pairscount.Scan(&count); err != nil {
			return nil, fmt.Errorf("splitDatabase: scan-count: %v", err)
		}
	}

	if count < m {
		err := errors.New("splitDatabase: count less than m")
		db.Close()
		return nil, err
	}
	var pairsperpartition float64
	pairsperpartition = float64(count) / float64(m)
	math.Ceil(pairsperpartition)
	//log.Printf("PairsperPartition: %v", pairsperpartition)
	rows, err := db.Query("SELECT key, value FROM pairs;")
	if err != nil {
		db.Close()
		return nil, fmt.Errorf("splitDatabase: db.Query(Select): %v", err)
	}
	defer rows.Close()
	i := 0
	j := 0
	var pathnames []string
	splitpath := fmt.Sprintf(outputPattern, j)
	pathnames = append(pathnames, splitpath)
	splitdb, err := createDatabase(splitpath)
	if err != nil {
		return nil, fmt.Errorf("splitDatabase: sql.Open: (split) %v", err)
	}
	_, err = splitdb.Exec("CREATE TABLE IF NOT EXISTS pairs (key text, value text);")
	if err != nil {
		return nil, fmt.Errorf("splitDatabase: db.Exec(CREATE TABLE): %v", err)
	}
	for rows.Next() {
		i++
		if i > int(pairsperpartition) {
			i = 0
			splitdb.Close()
			j++
			splitpath = fmt.Sprintf(outputPattern, j)
			pathnames = append(pathnames, splitpath)
			splitdb, err = createDatabase(splitpath)
			if err != nil {
				return nil, fmt.Errorf("splitDatabase: sql.Open: (split) %v", err)
			}
			_, err := splitdb.Exec("CREATE TABLE IF NOT EXISTS pairs (key text, value text);")
			if err != nil {
				return nil, fmt.Errorf("splitDatabase: db.Exec(CREATE TABLE): %v", err)
			}
		}
		var key string
		var value string
		if err := rows.Scan(&key, &value); err != nil {
			splitdb.Close()
			return nil, fmt.Errorf("splitDatabase: rows.Scan: %v", err)
		}
		if _, err := splitdb.Exec("INSERT INTO pairs (key,value) VALUES (?,?);", key, value); err != nil {
			splitdb.Close()
			return nil, fmt.Errorf("splitDatabase: db.Exec(INSERT INTO): %v", err)
		}
		//log.Printf("key,value: %v,%v", key, value)
	}
	splitdb.Close()
	return pathnames, nil
}

func mergeDatabases(urls []string, path string, temp string) (*sql.DB, error) {
	var outputdb *sql.DB
	var err error
	if outputdb, err = createDatabase(path); err != nil {
		return nil, fmt.Errorf("mergeDatabases:  %v", err)
	}
	for _, url := range urls {
		infile, err := download(url, temp)
		if err != nil {
			outputdb.Close()
			return nil, fmt.Errorf("mergeDatabases: %v", err)
		}
		err = gatherInto(outputdb, infile)
		if err != nil {
			outputdb.Close()
			return nil, fmt.Errorf("mergeDatabases: %v", err)
		}
	}
	return outputdb, nil
}
func download(URL, path string) (string, error) {
	// Get the data
	resp, err := http.Get(URL)
	if err != nil {
		return "", fmt.Errorf("download: http.Get error: %v", err)
	}
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("download: http.Get: not okay %v", resp.Body)
	}
	defer resp.Body.Close()
	log.Printf("URL: %v", URL)
	log.Printf("responsebody: %v", resp.Body)
	// Create the file
	fileURL, err := url.Parse(URL)
	if err != nil {
		return "", fmt.Errorf("download: url.Parse: %v", err)
	}
	filepath := fileURL.Path
	log.Printf("filepath: %v", filepath)
	segments := strings.Split(filepath, "/")
	fileName := segments[len(segments)-1]
	fullFilePath := path + "/" + fileName
	log.Printf("fullfile path: %v", fullFilePath)
	out, err := os.Create(fullFilePath)
	if err != nil {
		return "", fmt.Errorf("download: os.Create: %v", err)
	}
	defer out.Close()

	// Write the body to file
	_, err = io.Copy(out, resp.Body)
	if err != nil {
		return "", fmt.Errorf("download: io.Copy: %v", err)
	}
	return fullFilePath, err
}
func gatherInto(db *sql.DB, path string) error {
	_, err := db.Exec("attach ? as merge;", path)
	if err != nil {
		return fmt.Errorf("gatherInto: db.Exec(attach): %v", err)
	}
	_, err = db.Exec("insert into pairs select * from merge.pairs;")
	if err != nil {
		return fmt.Errorf("gatherInto: db.Exec(insert): %v", err)
	}
	_, err = db.Exec("detach merge")
	if err != nil {
		return fmt.Errorf("gatherInto: db.Exec(detach): %v", err)
	}
	if err = os.Remove(path); err != nil {
		return fmt.Errorf("gatherInto: os.Remove: %v", err)
	}
	return err
}
