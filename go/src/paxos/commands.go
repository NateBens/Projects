package main

import (
	"fmt"
	"log"
	"os"
)

var commands = make(map[string]func([]string, *Command))

func commandHandler(inputFields []string, command *Command) {
	input := inputFields[0]
	value, ok := commands[input]
	if ok {
		args := inputFields[1:]
		value(args, command)
	} else {
		fmt.Printf("[%s] Command not recognized\n", command)
	}
}
func addCommand(command string, action func([]string, *Command)) {
	commands[command] = action
}
func fillCommands() {
	addCommand("help", doHelp)
	addCommand("quit", doQuit)
	addCommand("get", doGet)
	addCommand("put", doPut)
	addCommand("delete", doDelete)
	addCommand("dump", doDump)
}

func doDump(args []string, command *Command) {
	if len(args) != 0 {
		log.Printf("'dump' takes no arguments.")
		return
	}
	log.Printf("Dump of replica :%v", port)
	log.Printf("Cell: %v", replica.Cell)
	nextundecidedslot := len(replica.Slots)
	for i := 0; i < len(replica.Slots); i++ {
		if !replica.Slots[i].Decided {
			nextundecidedslot = i
			break
		}
	}
	log.Printf("Next Unapplied slot: %v", nextundecidedslot)
	log.Println("Slots:")
	for i := 0; i < len(replica.Slots); i++ {
		log.Printf("	%v", replica.Slots[i])
	}
	log.Println("Database")
	for k, v := range replica.Database {
		log.Printf("	[%v] => [%v]\n", k, v)
	}
}

func doGet(args []string, command *Command) {
	if len(args) != 1 {
		log.Printf("'get' requires one argument, a key.")
		return
	}
	proposer(command)
}
func doDelete(args []string, command *Command) {
	if len(args) != 1 {
		log.Printf("'delete' requires one argument, a key.")
		return
	}
	proposer(command)
}
func doPut(args []string, command *Command) {
	if len(args) != 2 {
		log.Printf("'put' requires two arguments, a key, and a value.")
		return
	}
	proposer(command)
}

func doHelp(args []string, command *Command) {
	fmt.Printf("List of Commands:\n")
	for k := range commands {
		fmt.Printf("  [%s] \n", k)
	}
}
func doQuit(args []string, command *Command) {
	fmt.Printf("Goodbye!\n")
	os.Exit(0)
}
