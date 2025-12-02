#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

// 맵 및 게임 요소 정의 (수정된 부분)
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20
#define MAX_STAGES 2
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가
#define MAX_LIFE 3 // 생명 개수(기능구현3)

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];
int player_x, player_y;
int respawn_x, respawn_y; // 리스폰 위치 저장(기능구현3)
int stage = 0;
int life = MAX_LIFE; // 생명 변수 추가(기능구현3)
int score = 0;

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

// 터미널 설정
struct termios orig_termios;

// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input);
void move_player(char input);
void respawn(); // 리스폰 함수 추가(기능구현3)
void move_enemies();
void check_collisions();
void press_any_key(); // (미셸-기능구현4)
void show_title_screen(); // 한수 추가 (미셸-기능구현4)
char show_game_over_screen(int score); // 함수 추가 (미셸-기능구현4)
char show_ending_screen(int score); // 함수 추가 (미셸-기능구현4)
char wait_for_q_or_r(); // 함수 추가 (미셸-기능구현4-1)
int kbhit();

int main() {
    srand(time(NULL));
    enable_raw_mode();
    show_title_screen(); // 타이틀 화면 표시 (미셸-기능구현4)
    load_maps();
    init_stage();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        if (kbhit()) {
            c = getchar();
            if (c == 'q') {
                game_over = 1;
                continue;
            }
            if (c == '\x1b') {
                getchar(); // '['
                switch (getchar()) {
                case 'A': c = 'w'; break; // Up
                case 'B': c = 's'; break; // Down
                case 'C': c = 'd'; break; // Right
                case 'D': c = 'a'; break; // Left
                }
            }
            while (kbhit()) getchar();  // 입력 버퍼 완전 삭제 (미셸)
        }
        else {
            c = '\0';
        }

        update_game(c);
        draw_game();
        usleep(90000);
        // 생명이 0일때 게임 오버
        if (life <= 0) {
            game_over = 1;
            /*printf("\x1b[2J\x1b[H");
            printf("GAME OVER!\n");
            printf("FINAL SCORE: %d\n",score);*/
        }

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            }
            else {
                game_over = 1;
                /*printf("\x1b[2J\x1b[H");
                printf("축하합니다! 모든 스테이지를 클리어했습니다!\n");
                printf("최종 점수: %d\n", score);*/
            }
        }
    }

    char end_choice;

    if (life <= 0) {
        end_choice = show_game_over_screen(score);
    }
    else if (stage >= MAX_STAGES) {
        end_choice = show_ending_screen(score);
    }
    else {
        end_choice = 'q';  // q로 나간 경우 (미셸-기능구현4-1)
    }

    if (end_choice == 'r') { // 다시 시작인지 종료인지 확인 (미셸-기능구현4-1)
        // 모든 상태 초기화 후 재시작 (미셸-기능구현4-1)
        stage = 0;
        score = 0;
        life = MAX_LIFE;

        init_stage();
        return main();  // 메인 다시 실행 (미셸-기능구현4-1)
    }

    disable_raw_mode();
    return 0;
}



// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// 맵 파일 로드
void load_maps() {
    FILE* file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0;
            strncpy(map[s][r], line, MAP_WIDTH + 1);
            r++;
        }
    }
    fclose(file);
}


// 현재 스테이지 초기화
void init_stage() {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;
    life = MAX_LIFE; //스테이지 이동시 생명 개수 초기화(기능구현3)

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') {
                player_x = x;
                player_y = y;
                respawn_x = x; //리스폰 위치 저장(기능구현3)
                respawn_y = y; // 같음(기능구현3)
            }
            else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){ x, y, (rand() % 2) * 2 - 1 };
                enemy_count++;
            }
            else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){ x, y, 0 };
            }
        }
    }
}

//시작지점 리스폰 함수(기능구현3)
void respawn() {
    player_x = respawn_x; // 플레이어 위치 초기화
    player_y = respawn_y;
    is_jumping = 0; // 플레이어 상태 초기화
    velocity_y = 0;
    on_ladder = 0;
}

// 게임 화면 그리기
void draw_game() {
    printf("\x1b[2J\x1b[H");
    printf("Stage: %d | Score: %d | Life: ", stage + 1, score); // 화면에 생명 표시 추가(기능구현3)
    for (int i = 0; i < life; i++) {
        printf("♥ ");
    }
    printf("\n");
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");

    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            }
            else {
                display_map[y][x] = cell;
            }
        }
    }

    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input);
    move_enemies();
    check_collisions();
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H') || (current_tile == '#' && floor_tile == 'H');

    switch (input) {
        case 'a': 
            next_x--; 
            break;
            
        case 'd': 
            next_x++; 
            break;
            
        case 'w': 
            if (on_ladder) next_y--; 
            break;
        
        case 's': 
            if (on_ladder && (player_y + 1 < MAP_HEIGHT)) {
                if (floor_tile != '#') next_y++;
            }
            else if (floor_tile == 'H' && (player_y + 1 < MAP_HEIGHT)) {
                next_y++;
            }
            break;

        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
            }
            break;
    }

    if (next_x >= 0 && next_x < MAP_WIDTH && map[stage][player_y][next_x] != '#') {
        player_x = next_x;
    }
    
    if (on_ladder && (input == 'w' || input == 's')) {
        if (next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 
    else {
        if (is_jumping) {
            int move_amount = velocity_y; // velocity_y가 중력계산으로 인한 변화 때문에 충돌 이전 값이 아닌 다음 값 사용, 로직 내 증감 없는 새로운 변수 추가
            int actual_move = 0; // 실제 이동거리 변수 추가
            char above_tile = (player_y - 1 >= 0) ? map[stage][player_y - 1][player_x] : ' '; // 플레이어 위 타일, 맵밖이면 ' '
            int jumped_from_ladder = (current_tile == 'H' && above_tile == '#'); // 사다리 위에 있고 머리 위가 #일때 
            
            if (move_amount < 0) { // 점프 중일 때
                for (int i = 0; i > move_amount; i--) { // 반복문으로 한 칸 이동할때마다 #과 충돌 발생 확인
                    int check_y = player_y + actual_move - 1; // 다음 이동 좌표 확인
                    if (check_y >= 0 && check_y < MAP_HEIGHT) {
                        char check_tile = map[stage][check_y][player_x];
                        
                        if (check_tile == '#') {
                            if (jumped_from_ladder) { 
                                jumped_from_ladder = 0; // //사다리 맨 위 H에서 점프시에는 위에 #이 있다면 블럭 관통이 한번만 일어나야하기 때문에 0으로 변경
                                actual_move--;
                            } else {
                                velocity_y = 1; // 일반적인 점프 상황에서는 velocity 값을 바꿔서 다음 프레임부터는 낙하
                                break;
                            }
                        } else {
                            actual_move--; //블록이 없다면 한칸 더 위로 이동
                        }
                    } else {
                        break;
                    }
                }
            } else if (move_amount > 0) {
                for (int i = 0; i < move_amount; i++) {
                    int check_y = player_y + actual_move + 1;
                    if (check_y >= MAP_HEIGHT || map[stage][check_y][player_x] == '#') {
                        break;
                    }
                    actual_move++;
                }
            }
            next_y = player_y + actual_move;
            if (next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') { //맵안에 있고 #내부에 P가 들어있는 버그 사항이 아닐 때
                player_y = next_y; //실제 플레이어 이동
            }
            velocity_y++; // 다음 프레임 속도 증가
            if ((player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] == '#') {
                is_jumping = 0;
                velocity_y = 0;
            }
        } 
        else {
            if (floor_tile != '#' && floor_tile != 'H') {
                if (player_y + 1 < MAP_HEIGHT) {
                    player_y++;
                } else {
                    life--;
                    if (life > 0) {
                        respawn();
                    }
                }
            }
        }
    }
    
    if (player_y >= MAP_HEIGHT) {
        life--;
        if (life > 0) {
            respawn();
        }
    }
}


// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        }
        else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions() {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            life--; // 닿으면 생명 감소(기능구현3)
            score = (score > 50) ? score - 50 : 0;
            if (life > 0) { //생명>0이면 리스폰 함수 호출(기능구현3)
                respawn();
            }
            return;
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
        }
    }
}

// q 또는 r이 입력될 때까지 기다리고 그 값을 리턴 (미셸-기능구현4-1)
char wait_for_q_or_r() {
    char c = '\0';

    while (1) {
        if (kbhit()) {
            c = getchar();
            if (c == 'q' || c == 'r')
                return c;
        }
    }
}

//  타이틀 / 게임오버 / 클리어 화면 (미셸-기능구현4)
void press_any_key() {
    while (!kbhit()) {}
    getchar();
}

// 타이틀 화면 (NUGURI) (미셸-기능구현4)
void show_title_screen() {
    printf("\033[2J\033[1;1H");

    printf("           ▄▄   ▄ ▄    ▄   ▄▄▄  ▄    ▄ ▄▄▄▄▄  ▄▄▄▄▄          \n");
    printf("           █▀▄  █ █    █ ▄▀   ▀ █    █ █   ▀█   █            \n");
    printf("           █ █▄ █ █    █ █   ▄▄ █    █ █▄▄▄▄▀   █            \n");
    printf("           █  █ █ █    █ █    █ █    █ █   ▀▄   █            \n");
    printf("           █   ██ ▀▄▄▄▄▀  ▀▄▄▄▀ ▀▄▄▄▄▀ █    █ ▄▄█▄▄          \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                              \n");

    printf("             ▶ 아무 키나 눌러 게임을 시작하세요 ◀          \n");

    press_any_key();
}

// 게임 오버 화면 (GAME OVER) (미셸-기능구현4)
char show_game_over_screen(int score) {
    printf("\033[2J\033[1;1H");

    printf("   ▄▄▄    ▄▄   ▄    ▄ ▄▄▄▄▄▄         ▄▄▄▄  ▄    ▄ ▄▄▄▄▄▄ ▄▄▄▄▄ \n");
    printf(" ▄▀   ▀   ██   ██  ██ █             ▄▀  ▀▄ ▀▄  ▄▀ █      █   ▀█\n");
    printf(" █   ▄▄  █  █  █ ██ █ █▄▄▄▄▄        █    █  █  █  █▄▄▄▄▄ █▄▄▄▄▀\n");
    printf(" █    █  █▄▄█  █ ▀▀ █ █             █    █  ▀▄▄▀  █      █   ▀▄\n");
    printf("  ▀▄▄▄▀ █    █ █    █ █▄▄▄▄▄         █▄▄█    ██   █▄▄▄▄▄ █    █\n");
    printf("                                                               \n");
    printf("                                                               \n");
    printf("                                                               \n");
    printf("                 ▶ FINAL SCORE : %d ◀\n", score);
    printf("                                                               \n");
    printf("              r - 다시 시작   |   q - 종료                     \n");

    return wait_for_q_or_r();
}

// 클리어 화면 (CLEAR) (미셸-기능구현4)
char show_ending_screen(int score) {
    printf("\033[2J\033[1;1H");

    printf("             ▄▄▄  ▄      ▄▄▄▄▄▄   ▄▄   ▄▄▄▄▄    ▄            \n");
    printf("           ▄▀   ▀ █      █        ██   █   ▀█   █            \n");
    printf("           █      █      █▄▄▄▄▄  █  █  █▄▄▄▄▀   █            \n");
    printf("           █      █      █       █▄▄█  █   ▀▄   ▀            \n");
    printf("            ▀▄▄▄▀ █▄▄▄▄▄ █▄▄▄▄▄ █    █ █    █   █            \n");
    printf("                                                             \n");
    printf("                                                             \n");
    printf("                                                             \n");

    printf("                모든 스테이지 클리어! \n");
    printf("               ▶ FINAL SCORE : %d ◀\n", score);
    printf("                                                             \n");
    printf("                                                             \n");
    printf("              r - 다시 시작   |   q - 종료                   \n");
    return wait_for_q_or_r();
}


// 비동기 키보드 입력 확인
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}