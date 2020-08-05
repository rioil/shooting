#include <ncursesw/ncurses.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#define DEBUG

#define BULLET_MAX 5
#ifdef DEBUG
# define NOATTACK_TICK 500
#else
# define NOATTACK_TICK 5000
#endif

typedef struct Point {
  int x;
  int y;
} Point;

typedef struct Player {
  Point point;
  int max_hp;
  int hp;
  int score;
  int level;
} Player;

typedef struct Enemy {
  Point point;
  int max_hp;
  int hp;
  struct Enemy *prev;
  struct Enemy *next;
} Enemy;

typedef struct Bullet {
  Point point;
  struct Bullet *prev;
  struct Bullet *next;
} Bullet;

typedef struct GameStatus {
  int height;
  int width;
  int enemy_count;
  int bullet_count;
  int tick;
  int level;
  int noattack_tick;
} GameStatus;

void spawn_enemy(Enemy **list_head, GameStatus *status);
void shoot(Bullet **list_head, Player *player, GameStatus *status);
void show_player_data(Player player, GameStatus *status);
bool hit_check(Point point, Bullet *list_head, GameStatus *status);
void show_wall(GameStatus *status);
void gameover(Player *player, GameStatus *status);

const char enemy = '#';
const char const *str = "['_']-+=";
const char bullet = '*';

const char const *gameover_msg = "GAME OVER";

int main(int argc, char **argv){
	int	key;

  unsigned int interval = 0;

  GameStatus status = {0, 0, 0, 0, 0, 1, 0};
  Enemy *enemy_list_head = NULL;
  Bullet *bullet_list_head = NULL;

  Player player = {{0, 0}, 100, 100, 0, 1};

  setlocale(LC_ALL, "");
	initscr();
	noecho();		// echo();
	cbreak();		// nocbreak();
  curs_set(0);
  timeout(0);
  keypad(stdscr, TRUE);
	getmaxyx(stdscr, status.height, status.width);
	player.point.y = status.height / 2;
	player.point.x = status.width / 2 - strlen(str);

	while(1){
		erase();

    if(player.hp <= 0){
      timeout(-1);
      gameover(&player, &status);
      getch();
      endwin();
      return 0;
    }

		mvaddstr(player.point.y, player.point.x, str);

    status.tick++;
    status.noattack_tick++;
    if(status.tick > 100){ status.tick = 0; }
    interval++;

    bool update_bullet = !(status.tick % 10 == 0);
    Bullet *tmp_bullet = bullet_list_head;
    while(tmp_bullet){
      if(update_bullet){
        tmp_bullet->point.x++;
        if(tmp_bullet->point.x > status.width - 1){
          // 弾を消す
          if(tmp_bullet->next){
            tmp_bullet->next->prev = tmp_bullet->prev;
          }
          else{
            bullet_list_head = tmp_bullet->prev;
          }

          if(tmp_bullet->prev){
            tmp_bullet->prev->next = tmp_bullet->next;
          }

          Bullet *prev = tmp_bullet->prev;
          free(tmp_bullet);
          tmp_bullet = prev;

          status.bullet_count--;
          continue;
        }
      }

      mvaddch(tmp_bullet->point.y, tmp_bullet->point.x, bullet);
      tmp_bullet = tmp_bullet->prev;
    }

    spawn_enemy(&enemy_list_head, &status);

    Enemy *tmp = enemy_list_head;
    while(tmp){
      bool attack_succeeded = tmp->point.x < status.width / 2;
      if((update_bullet && hit_check(tmp->point, bullet_list_head, &status)) || attack_succeeded){
        if(tmp->next){
          tmp->next->prev = tmp->prev;
        }
        else{
          enemy_list_head = tmp->prev;
        }

        if(tmp->prev){
          tmp->prev->next = tmp->next;
        }

        Enemy *prev = tmp->prev;
        free(tmp);
        tmp = prev;

        status.enemy_count--;
        if(attack_succeeded){
          player.hp--;
          status.noattack_tick = 0;
        }
        else{
          player.score++;
          if(player.score > 0 && player.score % 10 == 0){ status.level++; }
        }
      }
      else{
        if(status.tick % (10 + 40 / status.level) == 0){ tmp->point.x--; }
        mvaddch(tmp->point.y, tmp->point.x, enemy);
        tmp = tmp->prev;
      }
    }

    show_wall(&status);
    show_player_data(player, &status);

		refresh();

		key = getch();
		if (key == 'q') break;

		switch(key){
      case KEY_UP:
      case 'w':
        player.point.y--;
        if(player.point.y < 0){ player.point.y = status.height - 2; }
        break;
      case KEY_DOWN:
      case 's':
        player.point.y++;
        if(player.point.y >= status.height - 1){ player.point.y = 0; }
        break;
      case KEY_LEFT:
      case 'a':
        player.point.x--;
        if(player.point.x < 0){ player.point.x = 0; }
        break;
      case KEY_RIGHT:
      case 'd':
        player.point.x++;
        if(player.point.x > (status.width / 2 - strlen(str))){ player.point.x = (status.width / 2 - strlen(str)); }
        break;
      case ' ':
        if(interval > 10){
          // 新しい弾をリストに追加して，描画
          shoot(&bullet_list_head, &player, &status);
          mvaddch(bullet_list_head->point.y, bullet_list_head->point.x, bullet);

          interval = 0;
        }
        break;
    }

    if(status.noattack_tick > NOATTACK_TICK){
      if(player.level < 5){ player.level++; }
      status.noattack_tick = 0;
    }

    usleep(10000);
	}
	endwin();
	return (0);
}

void spawn_enemy(Enemy **list_head, GameStatus *status){
  struct timeval t;
  gettimeofday(&t, NULL);
  long usec = t.tv_usec;

  srand(usec);
  int p = 200 - 5 * status->level;
  if(p <= 0){ p = 5; }
  if(rand() % p == 0){
    Enemy *new_enemy = (Enemy*)malloc(sizeof(Enemy));

    new_enemy->point.y = rand() % (status->height - 1);
    new_enemy->point.x = status->width - 1;
    new_enemy->max_hp = new_enemy->hp = 10;

    if(*list_head){
      (*list_head)->next = new_enemy;
    }

    new_enemy->prev = *list_head;
    new_enemy->next = NULL;

    *list_head = new_enemy;

    status->enemy_count++;
  }
}

void shoot(Bullet **list_head, Player *player, GameStatus *status){
  for(int i=1; i<=(2 * player->level - 1); i++){
    int y = player->point.y - player->level + i;
    if(y < 0 || y > status->height - 2){ continue; }

    Bullet *new_bullet = (Bullet*)malloc(sizeof(Bullet));

    new_bullet->point.y = y;
    new_bullet->point.x = player->point.x + strlen(str);

    if(*list_head){
      (*list_head)->next = new_bullet;
    }

    new_bullet->prev = *list_head;
    new_bullet->next = NULL;

    *list_head = new_bullet;

    status->bullet_count++;
  }
}

void show_player_data(Player player, GameStatus *status){
  char gauge[6];
  int i = 0;
  for(i=0; i<player.level; i++){
    gauge[i] = '=';
  }
  gauge[++i] = '\0';

  mvprintw(0, 0, "HP:%3d *:%-5s %4d", player.hp, gauge, status->noattack_tick);
  mvprintw(status->height - 1, 0, "LEVEL:%2d SCORE:%4d ENEMY:%d", status->level, player.score, status->enemy_count);
}

bool hit_check(Point point, Bullet *list_head, GameStatus *status){
  Bullet *tmp = list_head;

  while (tmp){
    if(tmp->point.x == point.x && tmp->point.y == point.y){
      return true;
    }
    tmp = tmp->prev;
  }
  return false;
}

void show_wall(GameStatus *status){
  int x = status->width / 2;
  for(int y=0; y < status->height - 1; y++){
    mvaddch(y, x, '|');
  }
}

void gameover(Player *player, GameStatus *status){
  mvaddstr(status->height / 2, (status->width - strlen(gameover_msg)) / 2, gameover_msg);
  mvprintw(status->height / 2 + 2, status->width / 2, "SCORE %d", player->score);
  flash();
}