/* Sarah Hwang, Wonseok Lee, Eamon Lopez 
netpong.cpp 
This project implements a Pong game that is played over a network between two players. */

#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstdlib>
#include <signal.h>
#include <csignal>

#define WIDTH 43
#define HEIGHT 21
#define PADLX 1
#define PADRX WIDTH - 2

// Global variables recording the state of the game
// Position of ball
int ballX, ballY;
// Movement of ball
int dx, dy;
// Position of paddles
int padLY, padRY;
// Player scores
int scoreL, scoreR;
bool endGame;
int roundCount;

/* Initialize thread */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER ;

/* Initialize struct for message */
struct message {
	int dx;
	int dy;
	int ballX;
	int ballY;
	int padLY;
	int padRY;
};

/* Variables for connection */
bool isHost;
int connectFD;
struct sockaddr_in sin;
char *port;

// ncurses window
WINDOW *win;

/* Draw the current game state to the screen
 * ballX: X position of the ball
 * ballY: Y position of the ball
 * padLY: Y position of the left paddle
 * padRY: Y position of the right paddle
 * scoreL: Score of the left player
 * scoreR: Score of the right player
 */
void draw(int ballX, int ballY, int padLY, int padRY, int scoreL, int scoreR) {
    // Center line
    int y;
    for(y = 1; y < HEIGHT-1; y++) {
        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);
    }
    // Score
    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);
    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);
    // Ball
    mvwaddch(win, ballY, ballX, ACS_BLOCK);
    // Left paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padLY - 2 && y <= padLY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADLX, ch);
    }
    // Right paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padRY - 2 && y <= padRY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADRX, ch);
    }
    // Print the virtual window (win) to the screen
    wrefresh(win);
    // Finally erase ball for next time (allows ball to move before next refresh)
    mvwaddch(win, ballY, ballX, ' ');
}

/* Return ball and paddles to starting positions
 * Horizontal direction of the ball is randomized
 */
void reset() {
    ballX = WIDTH / 2;
    padLY = padRY = ballY = HEIGHT / 2;
    // dx is randomly either -1 or 1
    dx = (rand() % 2) * 2 - 1;
    dy = 0;
    // Draw to reset everything visually
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Display a message with a 3 second countdown
 * This method blocks for the duration of the countdown
 * message: The text to display during the countdown
 */
void countdown(const char *message) {
    int h = 4;
    int w = strlen(message) + 4;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, message);
    int countdown;
    for(countdown = 3; countdown > 0; countdown--) {
        mvwprintw(popup, 2, w / 2, "%d", countdown);
        wrefresh(popup);
        sleep(1);
    }
    wclear(popup);
    wrefresh(popup);
    delwin(popup);
    padLY = padRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay
}

/* Perform periodic game functions:
 * 1. Move the ball
 * 2. Detect collisions
 * 3. Detect scored points and react accordingly
 * 4. Draw updated game state to the screen
 */
void tock() {
    // Move the ball
    ballX += dx;
    ballY += dy;
    
    // Check for paddle collisions
    // padY is y value of closest paddle to ball
    int padY = (ballX < WIDTH / 2) ? padLY : padRY;
    // colX is x value of ball for a paddle collision
    int colX = (ballX < WIDTH / 2) ? PADLX + 1 : PADRX - 1;
    if(ballX == colX && abs(ballY - padY) <= 2) {
        // Collision detected!
        dx *= -1;
        // Determine bounce angle
        if(ballY < padY) dy = -1;
        else if(ballY > padY) dy = 1;
        else dy = 0;
    }

    // Check for top/bottom boundary collisions
    if(ballY == 1) dy = 1;
    else if(ballY == HEIGHT - 2) dy = -1;
    
    // Score points
    if(ballX == 0) {
        scoreR = (scoreR + 1) % 100;
        reset();
		/* check for round end */
		if (scoreR == 2) {
			scoreR = 0;
			scoreL = 0;
			char buff [30];
			sprintf(buff, "ROUND %d WIN -->", roundCount);
			roundCount++;
			countdown(buff);
		}
		else
			countdown("SCORE -->");
    } else if(ballX == WIDTH - 1) {
        scoreL = (scoreL + 1) % 100;
        reset();
		/* check for round end */
		if (scoreL == 2) {
			scoreR = 0;
			scoreL = 0;
			char buff [30];
			sprintf(buff, "ROUND %d WIN <--", roundCount);
			roundCount++;
			countdown(buff);
		}
		else
			countdown("<-- SCORE");
    }
    // Finally, redraw the current state
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Listen to keyboard input
 * Updates global pad positions
 */
void *listenInput(void *args) {

	/* Signal for input thread */
	struct sigaction act;
	memset(&act, '\0', sizeof(act));
	sigaction(SIGTERM, &act, NULL) ;

    while(1) {
        switch(getch()) {
            case KEY_UP: if (isHost) padRY--;
             break;
            case KEY_DOWN: if (isHost) padRY++;
             break;
            case 'w': if (!isHost) padLY--;
             break;
            case 's': if (!isHost) padLY++;
             break;
            default: break;
       }
    }       
    return NULL;
}

/* set up receiver handler */
void *getMessage(struct sockaddr_in * sin) {
	char buff[BUFSIZ];
	int addrLen = sizeof(struct sockaddr_in);
	int byt_rec;
	if ((byt_rec = recvfrom(connectFD, buff, sizeof(buff), 0, (struct sockaddr *) sin, (socklen_t *)&addrLen)) == -1) {
		std::cerr << "Error receiving " << strerror(errno) << std::endl;
		std::exit(1); }
	buff[byt_rec] = '\0';
	char * ret = buff;
	return ret;
}

/* set up to listen for network */
void *listenNet(void * args){

	/* sig handler */
	struct sigaction act;
	memset(&act, '\0', sizeof(act));
	sigaction(SIGTERM, &act, NULL) ;

	while (1) {
		message msg = *((message*)getMessage(&sin));

		/* check if the game is over (when dx = 10000) */
		if (msg.dx == 10000) {endGame = true;}
		/* determine the player that initiated*/
		if (isHost){
			if (ballX < WIDTH / 2) {
				dx = msg.dx;
				dy = msg.dy;
				ballX = msg.ballX;
				ballY = msg.ballY;
			}
			padLY = msg.padLY;
		}
		else{
			if (ballX > WIDTH / 2) {
				dx = msg.dx;
				dy = msg.dy;
				ballX = msg.ballX;
				ballY = msg.ballY;
			}
			padRY = msg.padRY;
		}
	}
	return NULL;
}

void initNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);
    box(win, 0, 0);
    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);
    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);
}

/* set up send handler */
void sendMessage(struct sockaddr_in * sinPtr, void * toSend, int msgSize) {
	sin.sin_family = AF_INET ;
	int returnKey;
	if	((returnKey = sendto(connectFD,toSend, msgSize, 0, (struct sockaddr *) sinPtr, sizeof(struct sockaddr_in))) < 0) {
		std::cerr << "Error sending: " << strerror(errno) << std::endl;
		std::exit(1);
	}
}

/* Socket information */
int getSocket(char * port) {
	/* create struct */
	int sockfd;
	struct sockaddr_in sin;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET ;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	/* receive host information */
	struct addrinfo * results;
	int status;
	if( (status = getaddrinfo(NULL, port, &hints, &results)) != 0 ) {
		std::cerr << "Failure to get addr info: " << gai_strerror(status) << std::endl;
		std::exit(1);
	}

	/* socket and bind */
	if ((sockfd = socket(results->ai_family, results->ai_socktype, 0)) < 0) {
		std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
	}

	int bindResult;
	if ((bindResult = bind(sockfd, results->ai_addr, results->ai_addrlen)) == -1) {
		std::cerr << "Error creating bind: " << strerror(errno) << std::endl;
		close(sockfd);
		std::exit(1);
	}

	freeaddrinfo(results);
	return sockfd;
}

/* connection to host */
void connectToHost(char * hostName) {

	/* create socket on client side */
	int sockfd;
	if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ) {
		std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
	}

	connectFD = sockfd;
	struct addrinfo * dest;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;

	/* get host information */
	int status;
	if ((status = getaddrinfo(hostName, port, &hints, &dest)) != 0) {
		std::cerr << "Error getting address information from host: " << gai_strerror(status) << std::endl;
		std::exit(1);
	}

	struct sockaddr_in * hostAdr = (struct sockaddr_in * ) dest->ai_addr ;
	sin = *hostAdr;

	char msg[9] = "connect" ;
	/* create contact with host */
	sendMessage(&sin, (void *) msg, strlen(msg)+1) ;	
}

/* handles endgame */
void handler(int signal) {
	endGame = true;
	struct message M;
	M.dx = 10000;
	sendMessage(&sin, (void *)&M, sizeof(struct message));
}

int main(int argc, char *argv[]) {
	/* initialize variables */
	endGame = false;
	char *hostPort;
	char *hostName;
	int refresh;
	int numRounds;

	signal(SIGINT, handler);
	if (argc != 3){
        fprintf(stderr, "Usage (for host): %s --host <PORT> \nUsage (for guest): %s <HOSTNAME> <PORT>\n", argv[0], argv[0]);
        exit(1);
    }

	hostPort = argv[2];
	if (!strcmp(argv[1], "--host") ){
		connectFD = getSocket(hostPort);
	
		/* prompt difficulty */
		char difficulty[10]; 
		printf("Please select the difficulty level (easy, medium or hard): ");
		scanf("%s", &difficulty);
		if(strcmp(difficulty, "easy") == 0) refresh = 80000;
		else if(strcmp(difficulty, "medium") == 0) refresh = 40000;
		else if(strcmp(difficulty, "hard") == 0) refresh = 20000;
		else{
			printf("Please enter a valid difficulty level, terminating... \n");
			return 0;
		}

		/* prompt number of rounds */
		std::cout << "Please enter the maximum number of rounds to play: ";
		scanf("%d", &numRounds);

		std::cout << "Waiting for connections on port: " << hostPort << std::endl;

		/* connect to opponent */
		char * buf = (char *) getMessage(&sin);
		isHost = true;
	}else{
		hostName = argv[1];
		std::cout << "Connecting to " << hostName << " on port: " << hostPort << std::endl;
		isHost = false;
		port = hostPort;
		sin.sin_port = htons(atoi(port));
		connectToHost(hostName);
	}

	if (isHost == false) {
		/* Get Refresh Rate and numRounds Number */
		refresh = *((int *) getMessage(&sin)) ;
		numRounds = *((int *) getMessage(&sin)) ;
	}
	else {
		/* Send Refresh Rate and numRounds Number */
		sendMessage(&sin, (void *) &refresh, sizeof(int)) ;
		sendMessage(&sin, (void *) &numRounds, sizeof(int)) ;
	}

	roundCount = 1;
	// --------------------//
    // Process args
    // refresh is clock rate in microseconds
    // This corresponds to the movement speed of the ball

    // Set up ncurses environment
    initNcurses();

    // Set starting game state and display a countdown
    reset();
    countdown("Starting Game");
    
    // Listen to keyboard input in a background thread
    pthread_t pth;
    pthread_create(&pth, NULL, listenInput, NULL);

	/* listen to network */
	pthread_t pth0;
	pthread_create(&pth0, NULL, listenNet, NULL);

    // Main game loop executes tock() method every REFRESH microseconds
    struct timeval tv;
    while(roundCount <= numRounds && !endGame) {
        gettimeofday(&tv,NULL);
        unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;
		/* send game information */
		struct message M;
		M.dx = dx;
		M.dy = dy;
		M.padLY = padLY;
		M.padRY = padRY;
		M.ballX = ballX;
		M.ballY = ballY;
		sendMessage(&sin, (void *) &M, sizeof(struct message));

        tock(); // Update game state
        gettimeofday(&tv,NULL);
        unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;
        unsigned long toSleep = refresh - (after - before);
        // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()
        // In that case it's MUCH bigger because of overflow!
        if(toSleep > refresh) toSleep = refresh;
        usleep(toSleep); // Sleep exactly as much as is necessary
    }
 
	/* Show Game Ended message */
	char messg [25] = "Game Ended"; 
    int height = 4;
    int width = strlen(messg) + 4;

    WINDOW *popup = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);
    wclear(popup);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, messg);
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
    wrefresh(popup);

	/* gets rid of input that happens due to delay */
    padLY = padRY = HEIGHT / 2; 

	sleep(2);
    delwin(popup);
    endwin();

	/* kill threads */
	pthread_kill(pth, SIGTERM);
	pthread_kill(pth0, SIGTERM);

    // Clean up
    pthread_join(pth, NULL);
	pthread_join(pth0, NULL);
    return 0;
}