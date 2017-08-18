
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define CODE_LENGTH 4
#define COLOR_COUNT 8

/**** Code and Response ****/

typedef struct Code {
    char colors[CODE_LENGTH];
} Code ;

typedef struct Response {
    int fit;
    int misplaced;
} Response ;

void setCodeToZero(Code *code) {
    for(int i = 0; i < CODE_LENGTH; i++) {
        code->colors[i] = 0;
    }
}

void printCodeLetters(Code code) {
    //the colors are represented as letters ('a'..'h' given COLOR_COUNT 8)
    //the input for the players work the same
    for(int i = 0; i < CODE_LENGTH; i++) {
        printf("%c", code.colors[i] + 'a');
    }
}

//this function allows iterating all codes
int nextCode(Code *code) {
    //increment code "little endian"
    int i = 0;
    //increment cascades up to fitting spot
    while(code->colors[i] >= COLOR_COUNT - 1) {
        code->colors[i] = 0;
        i++;
    }

    //overflow occurred
    if(i >= CODE_LENGTH) return 0;

    code->colors[i]++;
    return 1;
}

size_t getCodeCount(void) {
    size_t result = 1;
    for(int i = 0; i < CODE_LENGTH; i++) {
        result *= COLOR_COUNT;
    }
    return result;
}

int readCode(char *str, size_t len, Code *code) {
    if(len != CODE_LENGTH) return 0;
    //disable read code when alphabet is not enough
    if(COLOR_COUNT > 26) return 0;

    for(size_t i = 0; i < CODE_LENGTH; i++) {
        if(str[i] < 'a' || str[i] >= 'a'+COLOR_COUNT) {
            return 0;
        }
        code->colors[i] = str[i] - 'a';
    }
    return 1;
}

Code randomCode(void) {
    Code code;
    for(size_t i = 0; i < CODE_LENGTH; i++) {
        code.colors[i] = rand() % COLOR_COUNT;
    }
    return code;
}

/*
 * The heart of the game. The player gets kind of distance measure
 * between his input and the secret code as response.
 *
 * One number shows how many color inputs are in the right spot (result.fit,
 * usually red response pins). Its the complement of the Hamming distance. The
 * other shows how many colors would be correct IF they were in different spot
 * (result.misplaced, white response pins).
 */
Response calcResponse(Code input, Code hidden) {
    /*
     * This is actually rather complicated because of the
     * possibility of multiple occurences of the same color in the codes.
     *
     * This code uses the idea that the sum of 'fit' and 'misplaced' is equal
     * to the cardinality of the multiset-intersection of the codes viewed as multisets
     * by disregarding the order of the colors.
     *
     * Calculating 'fit' is easy. 'misplaced' is the difference of that
     * cardinality and 'fit'.
     */

    Response result;
    int countColorsInput[COLOR_COUNT];
    int countColorsHidden[COLOR_COUNT];

    //set to 0
    for(int i = 0; i < COLOR_COUNT; i++) {
        countColorsInput[i] = 0;
        countColorsHidden[i] = 0;
    }

    //count colors
    for(int i = 0; i < CODE_LENGTH; i++) {
        countColorsInput[(size_t) input.colors[i]]++;
        countColorsHidden[(size_t) hidden.colors[i]]++;
    }

    //find how many matching colors are in input
    int matchingColors = 0;
    for(int i = 0; i < COLOR_COUNT; i++) {
        int countInput = countColorsInput[i];
        int countHidden = countColorsHidden[i];
        //maximum of matching colors is hidden's color count
        matchingColors += (countInput < countHidden) ? countInput : countHidden;
    }

    //find "fit"
    int fit = 0;
    for(int i = 0; i < CODE_LENGTH; i++) {
        if(input.colors[i] == hidden.colors[i]) {
            fit++;
        }
    }

    result.fit = fit;
    //misplaced is matchingColors which are not "fit"
    result.misplaced = matchingColors - fit;

    return result;
}

/**** Turns ****/

typedef struct Turn {
    Code code;
    Response response;
} Turn;

int codeFitsTurn(Code code, Turn turn) {
    Response resp = calcResponse(code, turn.code);
    return (resp.fit == turn.response.fit) && (resp.misplaced == turn.response.misplaced);
}

/**** Stack of Turns ****/

typedef struct TurnNode {
    Turn turn;
    struct TurnNode *prev;
} TurnNode ;

void pushTurn(TurnNode **turnStack, Turn turn) {
    TurnNode *newNode = malloc(sizeof(TurnNode));
    newNode->turn = turn;
    newNode->prev = *turnStack;
    *turnStack = newNode;
}

void popTurn(TurnNode **turnStack) {
    TurnNode *prev = (*turnStack)->prev;
    free(*turnStack);
    *turnStack = prev;
}

void freeTurns(TurnNode *turnStack) {
    while(turnStack != NULL) {
        popTurn(&turnStack);
    }
}

int codeFitsTurnStack(Code code, TurnNode *turnStack) {
    for(TurnNode *node = turnStack; node != NULL; node = node->prev) {
        if(!codeFitsTurn(code, node->turn)) return 0;
    }
    return 1;
}

unsigned int countPossibleCodes(TurnNode *turnStack, char *possibleCodeBools) {
    unsigned int count = 0;
    size_t i = 0;

    //possibleCodeBools is short cut/cache/filter
    //only codes corresponding to bools set are even checked

    Code code;
    setCodeToZero(&code);
    do {
        if(possibleCodeBools[i] && codeFitsTurnStack(code, turnStack)) {
            count++;
        }
        i++;
    } while(nextCode(&code));
    return count;
}

/**** Best Turn Calculation ****/

/*
 * Eval the quality of this possible guess by the worst-case number
 * of possible codes after the guess.
 *
 * This is a good approximation of the common min-max-algo for two player
 * games. It's essentialy doing one turn of min-max and then using the
 * heuristic of optimizing possibility space. Bonus points for using a possible
 * secret code (possibility of winning instantly).
 */
unsigned int evalGuess(Code guess, TurnNode *turnStack, char *possibleCodeBools) {
    unsigned result = 0;
    size_t i = 0;
    Code possibleSecret;
    setCodeToZero(&possibleSecret);
    do {
        if(possibleCodeBools[i]) {

            Response resp = calcResponse(guess, possibleSecret);

            //make temp node on stack
            TurnNode newNode = {{guess, resp}, turnStack};

            unsigned count = countPossibleCodes(&newNode, possibleCodeBools);

            //search for maximum
            if(count > result) result = count;
        }

        i++;
    } while(nextCode(&possibleSecret));

    return result;
}

/*
 * Brut-force the best guess (as measuerd by the function above)
 */
Code calcBestGuess(TurnNode *turnStack) {
    Code code;
    setCodeToZero(&code);
    size_t codeIndex = 0;

    unsigned bestGuessEval = UINT_MAX;
    Code bestGuess;
    setCodeToZero(&bestGuess);

    //create cache of all possible codes
    char *possibleCodeBools = malloc(getCodeCount());
    do {
        possibleCodeBools[codeIndex] = (char) codeFitsTurnStack(code, turnStack);
        codeIndex++;
    } while(nextCode(&code));

    //restart code iteration
    setCodeToZero(&code);
    codeIndex = 0;

    do {
        if(codeIndex % 5 == 0) {
            printf("Checking %d...\r", (int) codeIndex);
            fflush(stdout);
        }

        unsigned eval = evalGuess(code, turnStack, possibleCodeBools);

        //give 0.5 points for being a possible code
        //(normalized by times 2 to be integer)
        eval *= 2;
        eval -= possibleCodeBools[codeIndex];

        //find (arg) min
        if(eval < bestGuessEval) {
            bestGuessEval = eval;
            bestGuess = code;
        }
        codeIndex++;
    } while(nextCode(&code));

    //clean up
    printf("                            \r");
    free(possibleCodeBools);
    printf("%d\n", bestGuessEval / 2);

    return bestGuess;
}

/**** Helper ****/

int readCmd(FILE *file, char *buffer, size_t len) {
    char c = '\0';
    char *p = buffer;
    while(!feof(file)) {
        //no space
        if(p >= buffer + len - 1) return -1;

        c = getc(file);

        //internal error
        if(ferror(file)) return -1;

        //found end
        if(c == '\n' || c == EOF) break;

        //collect character
        *p = c;
        p++;
    }

    *p = '\0';
    return p - buffer;
}

int strStartsWith(char *str, char *prefix) {
    while(1) {
        if(*prefix == '\0') return 1;
        if(*str == '\0') return 0;
        if(*str != *prefix) return 0;

        str++;
        prefix++;
    }
}

/**** Main ****/

int main(int argc, char** argv) {
    printf("Solvemind:\n");

    /*
    {
        Code codes[] = {
            {{ 1, 2, 3, 4}},
            {{ 1, 2, 4, 3}},
            {{ 7, 2, 4, 4}},
            {{ 4, 2, 1, 8}}
        };

        for(int i = 0; i < 4; i++) {
            Response resp = calcResponse(codes[0], codes[i]);
            printf("%d: %d, %d \n", i, resp.fit, resp.misplaced);
        }
    }
    */

    /*
    {
        Code code = {0, 0, 0, 0};
        int i = 0;
        do {
            printf("Code #%d: %d, %d, %d, %d\n", i, code.colors[0], code.colors[1], code.colors[2], code.colors[3]);
            i++;
        } while(nextCode(&code));
    }
    */

    int quit = 0;
#define CMD_LEN 1024
    char cmd[CMD_LEN];
    size_t length;

    srand(time(NULL));
    Code secret = randomCode();

    TurnNode *turnStack = NULL;

    while(!quit) {
        printf("\n> ");
        if(( length = readCmd(stdin, cmd, CMD_LEN)) == -1) {
            return 1;
        }

        if(strStartsWith(cmd, ":q") || (strcmp(cmd, ":exit")) == 0) {
            quit = 1;
        }

        if(strStartsWith(cmd, ":rev")) {
            printCodeLetters(secret);
            printf("\n");
        }

        if(strStartsWith(cmd, ":pos")) {
            Code code;
            setCodeToZero(&code);

            int c = 0;
            do {
                if(!codeFitsTurnStack(code, turnStack)) continue;
                c++;
                printf("Code #%d: ", c);
                printCodeLetters(code);
                printf("\n");
            } while(nextCode(&code));
        }

        if(strcmp(cmd, ":best") == 0) {
            Code bestGuess = calcBestGuess(turnStack);
            printCodeLetters(bestGuess);
            printf("\n");
        }

        if(strcmp(cmd, ":new") == 0) {
            secret = randomCode();
            freeTurns(turnStack);
            turnStack = NULL;
        }

        if(strcmp(cmd, ":turns") == 0) {
            for(TurnNode *node = turnStack; node != NULL; node = node->prev) {
                printCodeLetters(node->turn.code);
                printf(" %d, %d\n", node->turn.response.fit, node->turn.response.misplaced);
            }
        }

        if(strcmp(cmd, ":pop") == 0) {
            if(turnStack) popTurn(&turnStack);
        }

        Code code;
        if(readCode(cmd, length, &code)) {
            Response resp = calcResponse(code, secret);
            printf("Fit %d, Misplaced %d\n", resp.fit, resp.misplaced);
            if(resp.fit == CODE_LENGTH) {
                printf("You've won!\n");
            } else {
                Turn turn;
                turn.response = resp;
                turn.code = code;
                pushTurn(&turnStack, turn);
            }
        }
    }

    freeTurns(turnStack);

}
