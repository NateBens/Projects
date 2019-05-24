package main

import (
	"bufio"
	"fmt"
	"log"
	"math/rand"
	"net"
	"net/http"
	"net/rpc"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

type Replica struct {
	Port     string
	Cell     []string
	Slots    []Slot
	Database map[string]string
	Mutex    sync.Mutex
}

type Slot struct {
	Number  int
	Command Command
	Decided bool
	Promise Sequence
}

func (elt Slot) String() string {
	var decidedstring string
	if elt.Decided {
		decidedstring = "Decided"
	} else {
		decidedstring = "Undecided"
	}
	return fmt.Sprintf("[%v] %v %v, %v", elt.Number, decidedstring, elt.Command, elt.Promise)
}

type Sequence struct {
	Number  int
	Address string
}

func (elt Sequence) String() string {
	return fmt.Sprintf("N=%v/:%v", elt.Number, elt.Address)
}

type Command struct {
	Promise Sequence
	Command string
	Address string
	Tag     int
}

func (elt Command) String() string {
	return fmt.Sprintf("{'%v'}", elt.Command)
}
func (this Command) Equal(that *Command) bool {
	return this.Command == that.Command && this.Address == that.Address && this.Tag == that.Tag
}
func (this Command) GThan(that Command) bool {
	return this.Promise.Number > that.Promise.Number
}

var port string
var replica *Replica
var decisionmap = make(map[string]chan (string))

func main() {
	replica = new(Replica)
	log.Println("Nathan Benson: CS 3410, Spring 2019")
	log.Println("Welcome to Paxos!")
	log.Println("")
	args := os.Args[1:]
	if len(args) < 3 {
		log.Fatalf("This Implementation of paxos required at least three command line arguments, the port #'s of replicas in a cell.")
	}
	port = args[0]
	localaddress := getLocalAddress()
	log.Printf("Found Local Address: %v\n", localaddress)
	address := (localaddress + ":" + port)
	log.Printf("Address: %v\n", address)
	log.Println()
	//initialize replica object
	replica.Port = port
	replica.Cell = args
	/*for i := 0; i < len(replica.Cell); i++ {
		log.Printf("Cell[%v] = %v", i, replica.Cell[i])
	}
	log.Printf("len(cell) %v", len(replica.Cell))*/
	replica.Database = make(map[string]string)
	//Server
	go func() {
		if err := rpc.Register(replica); err != nil {
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
	}()

	//Shell
	log.Println("Starting shell")
	log.Println("type 'help' for list of commands")
	log.Println()
	fillCommands()
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		input := scanner.Text()
		var command *Command
		commandFields := strings.Fields(input)
		if len(commandFields) != 0 {
			if commandFields[0] != "help" && commandFields[0] != "quit" && commandFields[0] != "dump" {
				//check if command succeeded
				tag := rand.Intn(1000)
				command = new(Command)
				command.Tag = tag
				command.Address = port
				command.Command = input
				decisonchannel := make(chan string, 1)
				channelkey := address + strconv.Itoa(tag)
				decisionmap[channelkey] = decisonchannel
				go func() {
					var decisionstring string
					decisionstring = <-decisonchannel
					log.Printf(decisionstring)
					delete(decisionmap, channelkey)
					close(decisonchannel)
				}()
			}
			commandHandler(commandFields, command)
		} else {
			continue
		}
	}
}

func proposer(command *Command) {
	decided := false
	nextSlot := len(replica.Slots)
	for i := 0; i < len(replica.Slots); i++ {
		if !replica.Slots[i].Decided && replica.Slots[i].Command.Command == "" {
			nextSlot = i
			break
		}
	}
	var sequence Sequence
	sequence.Number = 1
	sequence.Address = port
	localaddress := getLocalAddress()
	failures := 0
	for !decided {
		if failures > 0 && replica.Slots[nextSlot].Decided {
			if replica.Slots[nextSlot].Command.Equal(command) {
				log.Printf("[%v] Propose: --> Command proposed is already command decided in slot, ending proposer.", nextSlot)
				return
			}
			nextSlot += 1
		}
		command.Promise = sequence
		log.Printf("[%v] Propose: --> starting prepare round with N=%v", nextSlot, sequence.Number)
		prepRequest := new(PrepareRequest)
		prepRequest.Seq = sequence
		prepRequest.Slot = nextSlot
		//prepResponse := new(PrepareResponse)
		highestN := sequence.Number
		var highestSeqCommand Command
		highestSeqCommand.Promise.Number = 0
		highestSeqCommand.Tag = 0
		yesVotes := 0
		noVotes := 0
		majority := (len(replica.Cell) / 2) + 1
		prepareSuccess := false
		majorityFound := false
		finished := make(chan struct{})
		x := 0
		for i := 0; i < len(replica.Cell); i++ {
			address := (localaddress + ":" + replica.Cell[i])
			go func() {
				prepResponse := new(PrepareResponse)
				//log.Printf("address: %v", address)
				callfailed := false
				if err := call(address, "Replica.Prepare", prepRequest, &prepResponse); err != nil {
					log.Printf("[%v] Propose: --> Prepare error calling replica %v: %v", nextSlot, address, err)
					log.Printf("[%v] Propose: --> Counting error as 'no' vote", nextSlot)
					noVotes += 1
					callfailed = true
				}
				if prepResponse.Promise.Number > highestN {
					highestN = prepResponse.Promise.Number
				}
				/*if prepResponse.Command.GThan(highestSeqCommand) {
					highestSeqCommand = prepResponse.Command
				}*/

				if !prepResponse.Okay && !majorityFound && !callfailed {
					log.Printf("[%v] Propose: -->'no' vote recieved with Promise: %v", nextSlot, prepResponse.Promise)
					noVotes += 1
				}
				if prepResponse.Okay && !majorityFound {
					if prepResponse.Command.Promise.Number > 0 {
						highestSeqCommand = prepResponse.Command
						log.Printf("[%v] Propose: -->'yes' vote recieved with accepted Command=%v", nextSlot, prepResponse.Command)
					} else {
						log.Printf("[%v] Propose: -->'yes' vote recieved with no accepted command", nextSlot)
					}
					yesVotes += 1
				}
				if yesVotes >= majority && !majorityFound {
					i = 999
					prepareSuccess = true
					majorityFound = true
					log.Printf("[%v] Propose: --> got a majority of 'yes' votes, ignoring any additional responses", nextSlot)
					finished <- struct{}{}
				}
				if noVotes >= majority && !majorityFound {
					i = 999
					majorityFound = true
					log.Printf("[%v] Propose: --> got a majority of 'no' votes, ignoring any additional responses", nextSlot)
					finished <- struct{}{}
				}
				x++
			}()
			if x == (len(replica.Cell) - 1) {
				finished <- struct{}{}
			}
		}
		<-finished
		//close(finished)
		if prepareSuccess { //Prepare sucess, move to accept round
			accRequest := new(AcceptRequest)
			accRequest.Seq = sequence
			accRequest.Slot = nextSlot
			accResponse := new(AcceptResponse)
			var accSuccess bool
			if highestSeqCommand.Promise.Number > 0 { // I learned about a command
				accRequest.Command = highestSeqCommand
				log.Printf("[%v] Propose: --> value(s) returned, adopting command i learned about", nextSlot)
				log.Printf("[%v] Propose: --> Starting accept round with %v Command=%v", nextSlot, sequence, highestSeqCommand)
				//accept command i learned about
				accSuccess, highestN = acceptRound(accRequest, accResponse)
			} else { // I did not learn about a command, accept my command
				//accRequest.Command = command
				highestSeqCommand.Address = command.Address
				highestSeqCommand.Command = command.Command
				highestSeqCommand.Tag = command.Tag
				highestSeqCommand.Promise = command.Promise
				accRequest.Command.Address = command.Address
				accRequest.Command.Command = command.Command
				accRequest.Command.Tag = command.Tag
				accRequest.Command.Promise = command.Promise
				log.Printf("[%v] Propose: --> no values returned, choosing my command", nextSlot)
				log.Printf("[%v] Propose: --> Starting accept round with %v Command=%v", nextSlot, sequence, command)
				accSuccess, highestN = acceptRound(accRequest, accResponse)
			}
			if accSuccess {
				log.Printf("[%v] Propose: --> Starting decide round with Command=%v", nextSlot, highestSeqCommand)
				if failures > 0 {
					go proposer(command)
				}
				decRequest := new(DecideRequest)
				decRequest.Slot = nextSlot
				decRequest.Command = accRequest.Command
				decRequest.Command.Promise = accResponse.Promise
				var junk Nothing
				for i := 0; i < len(replica.Cell); i++ {
					address := (localaddress + ":" + replica.Cell[i])
					go func() {
						if err := call(address, "Replica.Decide", decRequest, &junk); err != nil {
							log.Printf("[%v] Propose: --> Decide error calling replica %v: %v", nextSlot, address, err)
						}
					}()
				}
				decided = true
				return
			}
		}
		//else { // prepare failed, increase sequence number, wait, try again.
		failures++
		min := 5 * failures
		max := 10 * failures
		sleepTime := min + rand.Intn(max-min)
		log.Printf("Prepare failed, sleeping for %vs", (sleepTime / 100))
		//sequence.Number++
		sequence.Number = highestN + 1
		time.Sleep(time.Duration(sleepTime) * time.Millisecond)
		//}
	}
	return
}

func acceptRound(accRequest *AcceptRequest, accResponse *AcceptResponse) (bool, int) {
	localaddress := getLocalAddress()
	noVotes := 0
	yesVotes := 0
	highestN := accRequest.Seq.Number
	majority := (len(replica.Cell) / 2) + 1
	finished := make(chan struct{})
	success := false
	majorityFound := false
	x := 0
	for i := 0; i < len(replica.Cell); i++ {
		callfailed := false
		address := (localaddress + ":" + replica.Cell[i])
		go func() {
			//log.Printf("address: %v", address)
			if err := call(address, "Replica.Accept", accRequest, &accResponse); err != nil {
				log.Printf("[%v] Propose: --> Accept error calling replica %v: %v", accRequest.Slot, address, err)
				log.Printf("[%v] Propose: --> Counting error as 'no' vote", accRequest.Slot)
				noVotes += 1
				callfailed = true
			}
			if accResponse.Promise.Number > highestN {
				highestN = accResponse.Promise.Number
			}
			if accResponse.Okay && !majorityFound {
				log.Printf("[%v] Propose: --> 'yes' vote received", accRequest.Slot)
				yesVotes++
			}
			if !accResponse.Okay && !majorityFound && !callfailed {
				log.Printf("[%v] Propose: --> 'no' vote received", accRequest.Slot)
				noVotes++
			}
			if yesVotes >= majority && !majorityFound {
				i = 999
				//log.Printf("yesVotes =%v - majority =%v", yesVotes, majority)
				log.Printf("[%v] Propose: --> got a majority of 'yes' votes, ignoring any additional responses", accRequest.Slot)
				finished <- struct{}{}
				success = true
				majorityFound = true
			}
			if noVotes >= majority && !majorityFound {
				i = 999
				log.Printf("[%v] Propose: --> got a majority of 'no' votes, ignoring any additional responses", accRequest.Slot)
				finished <- struct{}{}
				majorityFound = true
			}
			x++
		}()
		if x == (len(replica.Cell) - 1) {
			finished <- struct{}{}
		}
	}
	<-finished
	//close(finished)
	return success, highestN
}

func getAddress() string {
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
