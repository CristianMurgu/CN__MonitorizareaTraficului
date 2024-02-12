
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <chrono>
#include <ctime>

#define NM 100
#define PORT 2727

using namespace std;
using namespace chrono;

extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

thData * td[10]; //parametru functia executata de thread
int td_count=0, td_order=0;

sqlite3* server_db;
sqlite3* server_news;
int static basic_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  return 0;
}
int static print_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  for(unsigned int i=0; i<argc; i++)
  {
    if(argv[i])
      printf("%s ", argv[i]);
    else
      printf("%s ", " ");
  }
  printf("\n");
  return 0;
}


void create_server_db()
{
  char * DbError=0;
  char buffer[1024];
  FILE* fd=fopen("server.txt","r");
  while(fgets(buffer, sizeof(buffer), fd))
  {
    sqlite3_exec(server_db, buffer, print_callback, 0, &DbError);
  }
  fclose(fd);
}

void create_server_news()
{
  char * DbError=0;
  char buffer[1024];
  FILE* fd=fopen("server_news.txt","r");
  while(fgets(buffer, sizeof(buffer), fd))
  {
    sqlite3_exec(server_news, buffer, print_callback, 0, &DbError);
  }
  fclose(fd);
}

bool is_num(char* info)
{
  for(unsigned int i=0;i<strlen(info)-1;i++)
  {
    if(info[i]<'0' || info[i]>'9')
    {
      return 0;
    }
  }
  return 1;
}

void prepare_input(char* msg, char* street, char* info, int length)
{
  bool first;
  for(int i=0;i<length;i++)
  {
    if(msg[i]!=' ')
    {
      street[i]=msg[i];
    }
    else
    {
      street[i]='\0';
      strcpy(info,msg+i+1);
      break;
    }
  }
}

typedef struct
{
  int result;
} Callback_data;

int static check_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  Callback_data* call=(Callback_data*)NotUsed;
  if(argv[0][0]=='1')
  {
    call->result=1;
  }
  else if(argv[0][0]=='0')
  {
    call->result=0;
  }
  return 0;
}

bool street_exists(char* street)
{
	if(strstr(street, "auto"))
		return 1;
  Callback_data call;
  char command[1024], *error=0;
  strcpy(command, "SELECT EXISTS (SELECT street_name FROM Map WHERE upper(street_name) = '");
  strcat(command, street);
  strcat(command, "');");
  sqlite3_exec(server_db, command, check_callback, &call, &error);
  return call.result;
} 

bool check_speed(char* street, char* speed)
{
  Callback_data call;
  char command[1024], *error=0;
  strcpy(command,"SELECT EXISTS (SELECT ");
  strcat(command,"speed_limit FROM Map where upper(street_name)='");
  strcat(command,street);
  strcat(command,"' AND (speed_limit-(event*speed_limit))>=");
  strcat(command, speed);
  strcat(command,");");
  sqlite3_exec(server_db,command,check_callback, &call, &error);
  return call.result;
}

void set_crash(char* street)
{
  char command[1024];
  strcpy(command, "UPDATE Map SET event = 0.25 WHERE upper(street_name) = '");
  strcat(command, street);
  strcat(command, "';");
  sqlite3_exec(server_db, command, print_callback, 0, 0);
}

void set_event(char* street)
{
  char command[1024];
  strcpy(command, "UPDATE Map SET event = 0.50 WHERE upper(street_name) = '");
  strcat(command, street);
  strcat(command, "';");
  sqlite3_exec(server_db, command, print_callback, 0, 0);
}


void set_normal(char* street)
{
  char command[1024];
  strcpy(command, "UPDATE Map SET event = 0.00 WHERE upper(street_name) = '");
  strcat(command, street);
  strcat(command, "';");
  sqlite3_exec(server_db, command, print_callback, 0, 0);
}

void clear_client(int idThread)
{
  for(unsigned int i=1; i<=td_count; i++)
  {
    if(td[i]->idThread==idThread)
    {
    	for(unsigned int j=i;j<td_count;j++)
    	{
      	td[j]->cl=td[j+1]->cl;
      	td[j]->idThread=td[j+1]->idThread;
      }
    }
  }
  td_count--;
}

void send_all(char* msgrasp)
{
  for(unsigned int i=1; i<=td_count;i++)
  {
    if(write(td[i]->cl, msgrasp, 100)<=0)
		{
		  printf("[Thread %d] ",td[i]->idThread);
			perror("[Thread]Eroare la write() catre client.\n");
		}
		else
			printf("[Thread %d]Mesajul a fost trasmis cu succes.\n",td[i]->idThread);	
  }
}

void send_one(int cl, int idThread, char* msgrasp)
{
  if(write(cl, msgrasp, 100)<=0)
	{
		printf("[Thread %d] ",idThread);
		perror("[Thread]Eroare la write() catre client.\n");
	}
	else
		printf("[Thread %d]Mesajul a fost trasmis cu succes.\n",idThread);	
}

typedef struct
{
	char text[100];
}Callback_data_text;

int static copy_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  Callback_data_text* call=(Callback_data_text*)NotUsed;
  strcpy(call->text, argv[0]);
  return 0;
}
int day;
auto start_time=system_clock::now();
auto current_time=system_clock::now();
int elapsed_seconds;

void get_location(char * street)
{
	Callback_data_text call;
	char command[1024], *error=0;
	strcpy(command, "SELECT street_name from Map where id=");
	char str[10];
	srand(time(0)+getpid());
	int id= 1 + (rand()%5);
	sprintf(str, "%d", id);
	strcat(command, str);
	strcat(command, ";");
	sqlite3_exec(server_db,command, copy_callback, &call, &error);
	strcpy(street, call.text);
	strcat(street, "\0");	
}

void get_weather(char * msgrasp)
{
	Callback_data_text call;
	char command[100], row_number[10], *error=0;
	strcpy(command, "SELECT weather from News where id=");
	sprintf(row_number, "%d", day);
	strcat(command, row_number);
	strcat(command, ";");
	sqlite3_exec(server_news,command, copy_callback, &call, &error);
	strcpy(msgrasp, call.text);
	strcat(msgrasp, "\0");
}

void get_sport(char * msgrasp)
{
	Callback_data_text call;
	char command[100], row_number[10], *error=0;
	strcpy(command, "SELECT sport from News where id=");
	sprintf(row_number, "%d", day);
	strcat(command, row_number);
	strcat(command, ";");
	sqlite3_exec(server_news,command, copy_callback, &call, &error);
	strcpy(msgrasp, call.text);
	strcat(msgrasp, "\0");
}

void get_gas(char * msgrasp)
{
	Callback_data_text call;
	char command[100], row_number[10], *error=0;
	strcpy(command, "SELECT gas_prices from News where id=");
	sprintf(row_number, "%d", day);
	strcat(command, row_number);
	strcat(command, ";");
	sqlite3_exec(server_news,command, copy_callback, &call, &error);
	strcpy(msgrasp, call.text);
	strcat(msgrasp, "\0");
}

static void *treat(void *); 
void raspunde(void *);

int main ()
{
	start_time=system_clock::now();
	srand(time(0)+getpid());
	day=1+(rand()%4);

	FILE* check_db=fopen("server.db","r");
  if(!check_db)
  {
    sqlite3_open("server.db", &server_db);
    create_server_db();
    fclose(check_db);
  }
  else
  {
    sqlite3_open("server.db", &server_db);
    fclose(check_db);
  }

  check_db=fopen("server_news.db","r");
  if(!check_db)
  {
    sqlite3_open("server_news.db", &server_news);
    create_server_news();
    fclose(check_db);
  }
  else
  {
    sqlite3_open("server_news.db", &server_news);
    fclose(check_db);
  }

  for(unsigned int i=0;i<10;i++)
	{
		td[i]=(struct thData*)malloc(sizeof(struct thData));	
	}

  struct sockaddr_in server;
  struct sockaddr_in from;	
  int sd; 
  int pid;
  pthread_t th[NM];
  

  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror ("[server]Eroare la socket().\n");
    return errno;
  }
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));
  
    server.sin_family=AF_INET;	
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    server.sin_port=htons(PORT);
  
  if(bind (sd, (struct sockaddr *) &server, sizeof(struct sockaddr))==-1)
  {
    perror ("[server]Eroare la bind().\n");
    return errno;
  }

  if(listen(sd, 2)==-1)
  {
    perror ("[server]Eroare la listen().\n");
    return errno;
  }
  
  start_time=system_clock::now();
  while (1)
  {
  	current_time=system_clock::now();
  	elapsed_seconds=duration_cast<seconds>(current_time-start_time).count();
  	if(elapsed_seconds>=60)
  	{
  		srand(time(0)+getpid());
  		day= 1+(rand()%4);
  		start_time=system_clock::now();
  	}
    int client;     
    int length= sizeof (from);

    printf ("[server]Asteptam la portul %d...\n",PORT);
    fflush (stdout);

    // client= malloc(sizeof(int));
    if ( (client = accept (sd, (struct sockaddr *) &from, (socklen_t *) &length)) < 0)
		{
	  	perror ("[server]Eroare la accept().\n");
	  	continue;
		}
	
		td_count++;
		td[td_count]->idThread=td_order++;
		td[td_count]->cl=client;

		pthread_create(&th[td_count], NULL, &treat, td[td_count]);	      
	}//while    
};				
static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
		raspunde((struct thData*)arg);
		close ((intptr_t)arg);
		return(NULL);		
};


void raspunde(void *arg)
{
  char msg[NM]; 
  char msgrasp[NM]=" ";
  char street[NM]="", info[NM];
	struct thData tdL; 
	tdL= *((struct thData*)arg);
	while(true)
	{
		current_time=system_clock::now();
  	elapsed_seconds=duration_cast<seconds>(current_time-start_time).count();
  	if(elapsed_seconds>=60)
  	{
  		srand(time(0)+getpid());
  		day= 1+(rand()%4);
  		start_time=system_clock::now();
  	}

		if (read (tdL.cl, &msg,NM) <= 0)
		{
			printf("[Thread %d]\n",tdL.idThread);
			perror ("Eroare la read() de la client.\n");
		}
		printf("desc:%d",tdL.cl);
		printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread, msg);
		bzero(msgrasp, NM);
  	bzero(street, NM);
  	bzero(info, NM);
 		printf ("[server]Mesajul a fost receptionat...%s\n", msg);
  	if(strstr(msg,"QUIT"))
  	{
    	strcpy(msgrasp, "goodbye\0");
    	send_one(tdL.cl,tdL.idThread, msgrasp);
    	clear_client(tdL.idThread);
    	break;
  	}
  	else if(strstr(msg, "WEATHER_NEWS"))
  	{
  		get_weather(msgrasp);
  		send_one(tdL.cl,tdL.idThread, msgrasp);
  	}
  	else if(strstr(msg, "SPORT_NEWS"))
  	{
  		get_sport(msgrasp);
  		send_one(tdL.cl,tdL.idThread, msgrasp);
  	}
  	else if(strstr(msg, "GAS_PRICES"))
  	{
  		get_gas(msgrasp);
  		send_one(tdL.cl,tdL.idThread, msgrasp);
  	}
  	else
  	{
    	prepare_input(msg,street,info,strlen(msg));
    	if(street_exists(street)==0)
    	{
      	strcpy(msgrasp, "this street doesn't exists\0");
      	send_one(tdL.cl,tdL.idThread, msgrasp);
    	}
    	else if(is_num(info))
    	{
    		if(strstr(street,"auto"))
    		{
    			get_location(street);
    		}
      	if(check_speed(street, info)==0)
      	{
      	  strcpy(msgrasp,"speed is not ok\0");
        	send_one(tdL.cl,tdL.idThread, msgrasp);
      	}
      	else
      	{
        	strcpy(msgrasp,"speed is ok\0");
        	send_one(tdL.cl,tdL.idThread, msgrasp);
      	}
    	}
    	else if(strstr(info, "crash"))
    	{
      	strcpy(msgrasp, "Crash on ");
      	strcat(msgrasp, street);
      	set_crash(street);
      	send_all(msgrasp);
    	}
    	else if(strstr(info, "sport_event"))
    	{ 
      	strcpy(msgrasp, "Sport event on ");
      	strcat(msgrasp, street);
      	set_event(street);
      	send_all(msgrasp);
    	}
    	else if(strstr(info, "normal"))
    	{
     		strcpy(msgrasp, "back to normal on ");
      	strcat(msgrasp, street);
      	set_normal(street);
      	send_all(msgrasp);
    	}
    	else
    	{
      	strcpy(msgrasp, "you've given incorrect info\0");
      	send_one(tdL.cl,tdL.idThread, msgrasp);
    	}
  	}
  }	      
};
