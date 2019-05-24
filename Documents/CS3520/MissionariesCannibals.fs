\ basic words
: 3dup dup 2over rot		 ( xyz -- xyzxyz ) ;
: 3drop drop drop drop		 ( xyz -- ) ; 
: pack swap 10 * + swap 100 * +	 ( 1 2 3 -- 123 ) ;
: unpack ( 123 -- 1 2 3 ) 
	dup 100 / swap 100 mod dup 10 / swap 10 mod ;
: printstate ( side m c -- ) 
	." [ " swap rot . . . ." ]" ;
	
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ StateMethods (conditions)
: startstate ( -- near m n ) 
	1 3 3 ;
: isgoal ( near m c -- bool )
	0 <> if drop drop 0  
		else
			0 = if 
				drop cr ." Goal State Found!!! " cr -1
			else drop 0 
	then then ;
	
variable successorcounter
0 successorcounter !
: incrementsuccessorcounter
	1 successorcounter +! 
	;
: resetsuccessorcounter
	0 successorcounter !
	;
		
: isvalidOLD ( near m c -- bool ) 
	3dup
	\ more cannibals than missionaries
	< if drop drop drop drop 0 
		else drop cr
			3dup
			\ more than 3 cannibals
			3 > if drop drop drop drop drop 0
				else 
				drop drop 
				\ more than 3 missionaries
				drop 3 > if drop 0 	
				\ valid case
				else drop -1
	then then then ;
	

	
\ actual is valid function
: isvalid2 ( near m c -- bool ) 
	3dup
		\ more than 3 cannibals
		3 > if drop drop drop drop drop 0
			else 
			drop drop 3dup
			\ more than 3 missionaries
			drop 3 > if drop drop drop drop 0 	
			else
			drop 3dup
			\ if there are 0 missionaries (valid case)
			drop 0 = if drop drop drop drop -1
			else drop
			\ if there are more cannibals than missionaries and more than 0 missionaries
			< if drop 0
			else
			drop -1
	then then then then 
	incrementsuccessorcounter
	;
	
\ beggining isvalid function that checks if number are > 0, then passes them to isvalid2 if they are
	: isvalid ( near m c -- bool or near m c  )
	3dup
	\ if cannibals is less than one 
	0 < if drop drop drop drop drop 0
		else
			drop drop 3dup drop
			0 < if drop drop drop drop 0 
				else drop isvalid2
	then then ;
		
			
: isvalidSUPEROLD ( near m c -- bool ) 
	< if 
		drop 0 
	else drop -1
	then ;
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \

\ stack structures
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ used stackmethods
\ create used counter
variable usedcounter
0 usedcounter !
variable candidatecounter
0 candidatecounter !
: usedstack ( n -- ) ( -- adr )
	create here cell+ , cells allot does> ;
\ create used stack called ustack
100 usedstack ustack
: addused ( n candidatestack -- )
	ustack swap over @ ! cell swap +! 1 usedcounter +! ;
: incrementcandidatecounter
	1 candidatecounter +! ;
: decrementcandidatecounter
	candidatecounter @ 1 - 
	candidatecounter ! ;
\ check if a number is a member of the used set
: isused ( n -- bool )
    \ loop through all set elements
    usedcounter                         ( n &usedcounter )
    @                                   ( n usedcounter )
	1 +
    0                                  ( n usedcounter 0 )
    ?do                                 ( n )
        \ compare n with elt i
        dup                             ( n n )
        ustack                          ( n n &used )
        i                               ( n n &used i )
        cells                           ( n n &used i*8 )
        +                               ( n n &used[i] )
        @                               ( n n used[i] )
        \ return true if its a match
        =                               ( n n==used[i] )
        if                              ( n )
            drop                        ( )
            -1                          ( true )
            unloop                      ( true )
            exit                        ( true )
        then                            ( n )
    loop                                ( n )
    \ return false
    drop                                ( )
    0                                   ( false )
;
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ Candidate stack methods
: candidatestack ( n -- ) ( -- adr )
	create here cell+ , cells allot does> ;
\ create candidatestack called cstack at compile time
100 candidatestack cstack
: pushcandidate ( n candidatestack -- )
	cstack swap over @ ! cell swap +! ;
: popcandidate ( -- n ) 
	decrementcandidatecounter
	cstack cell negate over +! dup @ swap over >=
	abort" [psuedostack underflow] " @ ;
\ add a state to the candidate stack if it is valid and new
\ report on the outcome: invalid, repeat, or fresh
: addcandidate ( near m c -- ) 
	3dup 
	isvalid ( near m c -- bool 1 or 0 ) 
	\ false is not valid
	0 = if swap rot cr ." invalid [ " . . . ." ]" 
	\ is valid
		else 
		3dup pack isused ( side m c -> n -> bool ) 
		\ false is used 
		0 <> if swap rot cr ." used    [ " . . . ." ]"
		\ is valid is not used 
			else  
				3dup swap rot cr ." Fresh   [ " . . . ." ]" 
				pack dup addused incrementcandidatecounter pushcandidate 
	then then ;
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ Bread crumb trail stack methods
variable trailcounter
0 trailcounter !
: incrementtrailcounter
	1 trailcounter +! ;
: decrementtrailcounter
	trailcounter @ 1 - 
	trailcounter ! ;
: trailstack ( n -- ) ( -- adr )
	create here cell+ , cells allot does> ;
\ create trailstack called tstack 
100 trailstack tstack
: pushtrail ( n trailstack -- )
	incrementtrailcounter
	tstack swap over @ ! cell swap +! ;
: poptrail ( lifo -- x ) 
	decrementtrailcounter
	tstack cell negate over +! dup @ swap over >=
	abort" [psuedostack underflow] " @ ;

\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ misc stack methods (mostly printing)
: pclear ( psuedostack -- ) dup cell+ swap ! ;
: pbounds ( lifo -- addr1 addr2 ) dup @ swap cell+ ;	
: printused (  --  )
    \ loop through all set elements
    usedcounter                         ( &usedcounter )
    @                                   ( usedcounter )
	1
	+
    0                                   ( usedcounter 0 )
    ?do                                 (  )
        \ 
        ustack                          ( &used )
        i                               ( &used i )
        cells                           ( &used i*8 )
        +                               ( &used[i] )
        @                               ( used[i] )
        .								( )
    loop                                ( )
;

: printcandidate (  --  )
    \ loop through all set elements
	cr ." candidates: " 
    candidatecounter                         ( &usedcounter )
    @                                   ( usedcounter )
	1
	+
    0                                   ( usedcounter 0 )
    ?do                                 (  )
        \ 
        cstack                          ( &used )
        i                               ( &used i )
        cells                           ( &used i*8 )
        +                               ( &used[i] )
        @                               ( used[i] )
		i 0 <> if 
		unpack swap rot cr ." [ " . . . ." ]"
		then
    loop                                ( ) 
	drop
	 ;
: printtrail (  --  )
    \ loop through all set elements
	cr ." solution found: " cr ." -------------- "
    trailcounter                         ( &usedcounter )
    @                                   ( usedcounter )
	1
	+
    0                                   ( usedcounter 0 )
    ?do                                 (  )
        \ 
        tstack                          ( &used )
        i                               ( &used i )
        cells                           ( &used i*8 )
        +                               ( &used[i] )
        @                               ( used[i] )
		i 0 <> if 
		unpack swap rot cr ." [ " . . . ." ]"
		then
    loop                                ( ) 
	drop
	 ;
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \
\ find all successor candidates for the given state, push them on stack
\ leaves the number of states added on the stack

: successorsnear ( near m c -- n ) 
	3dup 
	\ move one cannibal
		1 -  swap rot drop 0 swap rot addcandidate 
	3dup
	\ move 2 cannibals 
		2 - swap rot drop 0 swap rot addcandidate  
	3dup
	\ move one of each type 
		1 - swap 1 - swap rot drop 0 swap rot addcandidate
	3dup 
	\ move 1 missionary
		swap 1 - swap rot drop 0 swap rot swap addcandidate
	3dup 
	\ move 2 missionaries
		swap 2 - swap rot drop 0 swap rot swap addcandidate	
	;

: successorsfar ( near m c -- n ) 
	3dup 
	\ move one cannibal
		1 +  swap rot drop 1 swap rot addcandidate 
	3dup
	\ move 2 cannibals 
		2 + swap rot drop 1 swap rot addcandidate  
	3dup
	\ move one of each type 
		1 + swap 1 + swap rot drop 1 swap rot addcandidate
	3dup 
	\ move 1 missionary
		swap 1 + swap rot drop 1 swap rot swap addcandidate
	3dup 
	\ move 2 missionaries
		swap 2 + swap rot drop 1 swap rot swap addcandidate		
	;

: successors ( near m c -- n )
	3dup
	swap rot 
	\ call successorsnear if near==1
	dup 1 = if drop drop drop successorsnear
	\ call successorsfar if near!=1
	else drop drop drop successorsfar
	then pack ;
	
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ SEARCH / / / / / / / / / / / / / / /
: search ( -- )
	\ printcandidate stack
	printcandidate		( ) 
	\ pop a candidate state off the candidate stack
	popcandidate ( n )
	\ push a copy on the bread-crumb trail stack
	dup pushtrail dup   ( n n -- n n )
	\ check if if it is goal state
	unpack isgoal ( n near m c -- n bool )
	\ if is goal, print out the contents of the bread-crumb trail in order. this is the solution.
	if ( n )
		drop printtrail ( )
	\ if it is not the goal
	else ( n )
		\ generate successors
		resetsuccessorcounter ( n )
		unpack ( near m c )
\		cr .s ." break " cr 
		successors ( n )
		drop ( )
\		cr .s ." break " cr 
		\  for each successor call search recursively
		candidatecounter                    ( &successorcounter )
		@                                   ( successorcounter )
\		1									( successorcounter 1 )
\		+									( successorcounter+1 )
		0                                   ( successorcounter 0 )
\		cr .s ." break " cr 
		do                                 (  )
			cr
			recurse
			leave
		loop                                ( ) 
	then
	poptrail							( n )
	clearstack
	;
	
\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ START METHOD / / / / / / / / / / / / / / /
: start ( n --  )
	133
	\ clear the stacks
	cstack pclear tstack pclear ustack pclear 
	\ push start state onto candidatestack
	unpack addcandidate 
	search
	;


















