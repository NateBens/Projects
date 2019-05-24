package main

import (
	"fmt"
	"log"
	"math/rand"
	"net/rpc"
	"strconv"
	"strings"
	"time"
)

type Nothing struct{}

type PrepareRequest struct {
	Slot int
	Seq  Sequence
}
type PrepareResponse struct {
	Okay    bool
	Promise Sequence
	Command Command
}

func (replica *Replica) Prepare(request *PrepareRequest, response *PrepareResponse) error {
	replica.Mutex.Lock()
	defer replica.Mutex.Unlock()
	sleep(1000)

	log.Printf("[%v] Prepare: called with %v", request.Slot, request.Seq)
	if len(replica.Slots) <= request.Slot {
		for i := len(replica.Slots); i <= request.Slot; i++ {
			var newslot Slot
			newslot.Promise.Number = 0
			newslot.Number = i
			replica.Slots = append(replica.Slots, newslot)
		}
		/*var newslot Slot
		newslot.Promise.Number = 0
		newslot.Number = request.Slot
		replica.Slots = append(replica.Slots, newslot)*/
	}
	slot := replica.Slots[request.Slot]

	if slot.Decided {
		log.Printf("[%v] Prepare: --> (Slot already decided)", request.Slot)
	}
	response.Okay = request.Seq.Number > slot.Promise.Number
	//Okay = true
	if response.Okay {
		response.Promise = request.Seq
		//log.Printf("[%v] Prepare: --> response.Promise = %v, request.Seq = %v", request.Slot, response.Promise, request.Seq)
		replica.Slots[request.Slot].Promise = request.Seq
		response.Command = slot.Command
		//and whatever command it most recently accepted (using the Accept operation below).
		//if response.Promise.Number > 0 {
		if slot.Decided {
			log.Printf("[%v] Prepare: --> Promising %v with Command=%v", request.Slot, request.Seq, slot.Command)
		} else {
			log.Printf("[%v] Prepare: --> Promising %v with no command", request.Slot, request.Seq)
		}
	} else {
		log.Printf("[%v] Prepare: --> Rejecting %v because %v already promised", request.Slot, request.Seq, slot.Promise)
		response.Promise = slot.Promise
	}
	sleep(1000)
	return nil
}

type AcceptRequest struct {
	Slot    int
	Seq     Sequence
	Command Command
}
type AcceptResponse struct {
	Okay    bool
	Promise Sequence
}

func (replica *Replica) Accept(request *AcceptRequest, response *AcceptResponse) error {
	replica.Mutex.Lock()
	defer replica.Mutex.Unlock()
	sleep(1000)

	log.Printf("[%v] Accept: called with %v Command=%v", request.Slot, request.Seq, request.Command)
	response.Okay = request.Seq.Number >= replica.Slots[request.Slot].Promise.Number
	if response.Okay {
		replica.Slots[request.Slot].Command = request.Command
		response.Promise = request.Seq
		log.Printf("[%v] Accept: --> Accepting %v with Command=%v", request.Slot, request.Seq, request.Command)
	} else {
		response.Promise = replica.Slots[request.Slot].Promise
		log.Printf("[%v] Accept: --> Rejecting %v with Command=%v", request.Slot, request.Seq, request.Command)

	}
	sleep(1000)
	return nil
}

type DecideRequest struct {
	Slot    int
	Command Command
}

func (replica *Replica) Decide(request *DecideRequest, response *Nothing) error {
	replica.Mutex.Lock()
	defer replica.Mutex.Unlock()
	sleep(1000)

	log.Printf("[%v] Decide: called with Command=%v", request.Slot, request.Command)
	log.Printf("Request promise = %v", request.Command.Promise)
	log.Printf("slot promise = %v", replica.Slots[request.Slot].Promise)

	if replica.Slots[request.Slot].Decided && replica.Slots[request.Slot].Command.Command != request.Command.Command {
		log.Fatalf("[%v] Decide: --> PANIC, Decision contradiction, exiting program", request.Slot)
	}
	/*if !(replica.Slots[request.Slot].Command.Address == request.Command.Address && replica.Slots[request.Slot].Command.Tag == request.Command.Tag) {
		If this is the first time that this replica has learned about the decision for this slot,
		it should also check if it (and possibly slots after it) can now be applied.
	}*/

	replica.Slots[request.Slot].Promise = request.Command.Promise
	if !replica.Slots[request.Slot].Decided {
		replica.Slots[request.Slot].Command = request.Command
		replica.Slots[request.Slot].Decided = true
		var applicable bool
		for i := request.Slot; i < len(replica.Slots); i++ {
			if request.Slot > 0 {
				applicable = replica.Slots[i-1].Decided
			} else {
				applicable = true
			}
			if applicable && replica.Slots[i].Command.Command != "" {
				log.Printf("[%v] Decide: --> Applying Command=%v", request.Slot, replica.Slots[i].Command)
				commandFields := strings.Fields(replica.Slots[i].Command.Command)
				var decisionString string
				if commandFields[0] == "put" {
					replica.Database[commandFields[1]] = commandFields[2]
					decisionString = fmt.Sprintf("put: [%v] set to [%v]", commandFields[1], commandFields[2])
					//decisionString = commandFields[0] + " " + commandFields[1] + " " + commandFields[2]
				}
				if commandFields[0] == "get" {
					value := replica.Database[commandFields[1]]
					decisionString = fmt.Sprintf("get: [%v] found [%v]", commandFields[1], value)
				}
				if commandFields[0] == "delete" {
					value := replica.Database[commandFields[1]]
					delete(replica.Database, commandFields[1])
					decisionString = fmt.Sprintf("delete: [%v] => [%v] deleted", commandFields[1], value)
				}
				//replica.Slots[request.Slot].Decided = true
				localaddress := getLocalAddress()
				address := (localaddress + ":" + port)
				channelkey := address + strconv.Itoa(request.Command.Tag)
				decisonchannel, ok := decisionmap[channelkey]
				if ok {
					decisonchannel <- decisionString
				}
			}
		}
	}

	sleep(1000)
	return nil
}

func sleep(n int) {
	min := n
	max := n * 2
	sleepTime := min + rand.Intn(max-min)
	time.Sleep(time.Duration(sleepTime) * time.Millisecond)
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
