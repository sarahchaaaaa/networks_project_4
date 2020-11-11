# networks_project_4
This project implements a Pong game that is played over a network between two players. The host can choose the level of difficulty and the number of rounds. The host uses the "UP/DOWN" keys to move their paddle and the user uses the "W/S" keys to move their paddle. The player that reaches a score of "2" first will win that particular round and the next round, if there are any left, will begin after a short countdown. 
## Eamon Marmion Lopez (elopez7), Sarah Hwang (shwang5), Wonseok Lee (wlee11)
### Files ###
`netpong.cpp`</br>
`Makefile`</br>
`README.md`</br>

### How to Run ###
#### Host ####
`./netpong --host <portnumber>`
Controls using "UP/DOWN" keys
#### Player ####
`./netpong HOSTNAME <portnumber>`
Controls using "W/S" keys
