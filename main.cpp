#include <iostream> 

using namespace std; 


/*
Notes:

Your creative design thinking process should coincide with Distributed Operating Systems.
You can use C, C++ and Java as a language while you want to code the problems.
If you feel about the randomization then you have to use only srand(). If you had any feeling regarding the non-preemption and preemption, then use only round robin scheduling with preemption.
Take all the assumptions whenever and wherever it is necessary and just record them in a file.
Use the following or equivalent code for implementing the Clock.
10 points are for creative thinking.
 

Clock:

#include <iostream>
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;

int main()
{
    auto t1 = Clock::now();
    auto t2 = Clock::now();
    std::cout << "Delta t2-t1: " 
              << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()
              << " nanoseconds" << std::endl;
}
 

Problem Statement: Philosophers are placed at the nodes of an undirected graph G, with one philosopher at each node. Neighboring philosophers communicate via messages subject to varying but finite transmission delays. Associated with each edge in G is a bottle shared between the two philosophers that are incident on the edge. A philosopher is either tranquil, thirsty, or drinking. A philosopher can be tranquil for an arbitrary period of time. A tranquil philosopher may become thirsty and need a nonempty subset of the bottles associated with his incident edges. He remains thirsty until he gets all the bottles he needs, at which point he starts drinking. He drinks for a finite time, after which he becomes tranquil and needs no bottles. A philosopher may need different subsets of bottles in different drinking sessions. Philosophers can drink concurrently from different bottles. The goal is to provide a solution which ensures that no two philosophers drink simultaneously from the same bottle, and that no philosopher remains thirsty forever.

Write an algorithm/pseudocode as a proof of your work.
Using POSIX threads, mutex locks, and semaphores (in conjunction with monitors-if possible) write a program for the third readers-writers problem including clocks keeping in the mind that you must adhere to happens-before relationship according to the Lamport’s Vector Logical Clock.
Provide the test suite and the results of the Alpha Testing.
Provide the test suite and the results of the Beta Testing.

A L G O R I T H M F O R T H E B A S I C P R O B L E M
Philosopher u maintains two nondecreasing integer variables, s _ n u m u and
r n a x . _ r e c u . s _ n u m u , referred to as u ' s session number, identifies u ' s last drinking ses-
sion if u is tranquil, u ' s upcoming drinking session if u is thirsty, and u ' s current
drinking session if u is drinking, m a x r e c u indicates the highest session number
received by u from his neighbors so far.
( s _ _ n u m u , u ) is referred to as u ' s extended session number.
( numu, u) < v) iff numu < _num, or numu= _numv and u < v
[5,8]. Each philosopher has an id which is different from the id's of his neighbors. (Ids
of nonneighboring philosophers may be the same.) Thus, if u and v are neighbors, their
extended session numbers are never equal.
Let u and v be two philosophers who share bottle b. Associated with bottle b is
a request token r e q b . Upon becoming thirsty, u sets s _ n u r n u to a value higher than
m a x _ r e c u , and if u needs and does not hold the bottle b, u sends to v the request
token for b, together with his extended session number, in the request message
(reqb , s _ n u m u , u ) . u sends request messages c o n c u r r e n t l y to all his neighbors with
whom he shares bottles he needs and does not hold. When the neighbor v receives a
85
request (reqb , s , u ), he obeys the following conflict resolution rule:
if v does not need b or (v is thirsty and (s, u ) < ( s _ n u m v , v)), then v immedi-
ately releases b.
If v does not release b immediately, then he needs b and will release it once he com-
pletes drinking from it. v will next use reqb only after he has released b. Thus, when
reqb is in transit from v to u, b will either be in transit from v to u ahead of reqb ,
or will already be at u. s...num u does not change while u is thirsty. Therefore, once
b is released by v, it will not return to v before u drinks from it.
Initially, every philosopher u is tranquil and s . n u m u---~-max_rec u = 0 . For every
bottle b shared between two philosophers, one philosopher has b and the other has
req b .
Each philosopher u obeys the rules given in Table 1 below. Each rule execution
is considered an atomic action. In addition to s...num u and m a x _ r e c u , each philoso-
pher u has the boolean variables n e e d u (b ), hold u ( b ), hold u (reqb ), indicating whether
u needs b, holds b, and holds reqb , respectively. Below, Send(b) and
Send(reqb , s.._numu, u ) send the specified message to the philosopher with whom b is
shared.
RI: becoming thirsty
R2: start drinking
R3: becoming tranquil,
and honoring
deferred requests
R4: requesting a bottle
Rb: receiving a request,
and
resolving a conflict
R6: receiving a bottle
when tranquil and "want to drink" do
become thirsty;
for each desired bottle b do n e e d u (b) *--- true;
s _ n u m u +---m a x rec u ÷ 1
when thirsty and holding all needed bottles do
become drinking
when drinking and "want to stop drinking" do
become tranquil;
for each consumed bottle b do
[need u (b ) ~ false;
if holdu(reqb ) then [Send(b ); holdu(b )+--false]]
when n e e d u ( b ), --,holdu(b ), holdu(reqb ) do
Send(reqb , s . . n u m u , u ); holdu(reqb ) ~----false
upon reception of (reqb , s , v ) do
hold u (reqb ) *--- true;
max._rec u ~ m a x ( m a x .recu, s );
if-~needu(b ) or (thirsty and ( s , v ) < ( s . . _ n u m u , u ) )
then [Send(b ); hold u ( b ) ~-- false]
upon reception of (b) do
hold u ( b ) ~-- true
Table 1. Basic Algorithm: Rules for philosopher u
86
*/

int main() 
{

}