
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/poll.h>
#include <chrono>
#include <ctime>
#include <cctype>

using namespace std;
using namespace chrono;

extern int errno;

int port;

auto start_time=system_clock::now();
auto current_time=system_clock::now();
int elapsed_seconds;

int max_speed;

int main (int argc, char *argv[])
{
  int sd;
  struct sockaddr_in server; 
  char msg[100];
  int msglen=0,length=0;

  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  port = atoi (argv[2]);

  if((sd=socket(AF_INET, SOCK_STREAM, 0))==-1)
  {
    perror ("Eroare la socket().\n");
    return errno;
  }

  server.sin_family=AF_INET;
  server.sin_addr.s_addr=inet_addr(argv[1]);
  server.sin_port=htons(port);

  if(connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr))==-1)
  {
    perror ("[client]Eroare la connect().\n");
    return errno;
  }

  struct pollfd file_descriptor[2];
  int delay=124;
  file_descriptor[0].fd=STDIN_FILENO;
  file_descriptor[0].events=POLLIN;
  file_descriptor[1].fd=sd;
  file_descriptor[1].events=POLLIN;

  char string_max_speed[5];
  srand(time(0)+getpid());
  max_speed= 40+(rand()%160);
  int current_speed;

  printf ("[client]You have this commands available:\n");
  printf("For reporting an event: street_name event_type(crash/sport_event/normal\n");
  printf("For getting news: weather_news/sport_news/gas_prices\n");
  while(true)
  {
    poll(file_descriptor, 2, delay);
    current_time=system_clock::now();
    elapsed_seconds=duration_cast<seconds>(current_time-start_time).count();
    if(elapsed_seconds>=40)
    {
      strcpy(msg,"auto ");
      srand(time(0)+getpid());
      current_speed=rand()%max_speed;
      sprintf(string_max_speed, "%d", current_speed);
      strcat(msg, string_max_speed);
      strcat(msg, "\0");
      write(sd,&msg,100);
      start_time=system_clock::now();
    }
    if(file_descriptor[0].revents & POLLIN)
    {
      bzero (msg, 100);
      fgets(msg, 100, stdin);
      for(unsigned int i=0;i<strlen(msg);i++)
      {
        if(msg[i]==' ' || msg[i]==0)
          break;
        else
          msg[i]=toupper(msg[i]);
      }
      if(write(sd,&msg,100)<=0)
      {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
      }
    }
    if(file_descriptor[1].revents & POLLIN)
    {
      fflush(stdout);
      if((read (sd, &msg,100))<0)
      {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
      }
      printf ("[client]Mesajul primit este: %s\n", msg);
      if(strstr(msg, "goodbye\0"))
        break;
    }
  }
  close (sd);
  return 0;
}
 