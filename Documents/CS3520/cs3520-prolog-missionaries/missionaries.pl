change(near,far).
change(far,near).


valid([M,C,near]):- (M=<3), (M>=0), (C=<3), (C>=0).
valid([M,C,far]):- (M=<3), (M>=0), (C=<3), (C>=0).

safe([M,C,_]):- (M>=C ; M = 0) , ((3-M)>=(3-C) ; M=3).





%move([A,C,X],onemissionary,[B,C,Y]) :-
%	change(X,Y) , (B is (A-1) ; B is (A+1)).
%move([A,C,X],twomissionaries,[B,C,Y]) :-
%	change(X,Y) , (B is (A-2) ; B is (A+2)).
%move([M,A,X],onecannibal,[M,B,Y]) :-
%	change(X,Y) , (B is (A-1) ; B is (A+1)).
%move([M,A,X],twocannibals,[M,B,Y]) :-
%	change(X,Y) , (B is (A-2) ; B is (A+2)).
%move([A,C,X],oneofeach,[B,D,Y]) :-
%	change(X,Y) , (B is (A-1) , D is (C-1) ; B is (A+1) , D is (C+1)).

move([A,C,near],onemissionary,[B,C,far]) :-
	(B is (A-1)).
move([A,C,near],twomissionaries,[B,C,far]) :-
	(B is (A-2)).
move([M,A,near],onecannibal,[M,B,far]) :-
	(B is (A-1)).
move([M,A,near],twocannibals,[M,B,far]) :-
	(B is (A-2)).
move([A,C,near],oneofeach,[B,D,far]) :-
	(B is (A-1) , D is (C-1)).
	
move([A,C,far],onemissionary,[B,C,near]) :-
	(B is (A+1)).
move([A,C,far],twomissionaries,[B,C,near]) :-
	(B is (A+2)).
move([M,A,far],onecannibal,[M,B,near]) :-
	(B is (A+1)).
move([M,A,far],twocannibals,[M,B,near]) :-
	(B is (A+2)).
move([A,C,far],oneofeach,[B,D,near]) :-
	(B is (A+1) , D is (C+1)).
	
	
solution([0,0,_],[]).
solution(Config,[Move|Rest]):-
	move(Config,Move,NextConfig),
	safe(NextConfig),
	valid(NextConfig),
	solution(NextConfig,Rest).