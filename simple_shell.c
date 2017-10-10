#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define LINESIZE 1000
#define PIPECAP 10
#define ARGCAP 50

int should_wait = 1;

void just_wait()
{
    while (wait(NULL) != -1);
}


void print_test( char static_args[PIPECAP][ARGCAP][100], char infile[50], char outfile[50], int pipe_no )
{
    for (int arg = 0; arg <= pipe_no; arg++)
    {
        for (int a = 0; strcmp(static_args[arg][a], "") != 0; a++)
        {
            printf("arg %d %d: '%s'\n", arg, a, static_args[arg][a]);
        }
    }
    printf("inf: <%s>\noutf: <%s>\n", infile, outfile);
}


int is_stopping_char( char c )
{
    switch (c)
    {
        case ' ':
        case '\0':
        case '\n':
        case '>':
        case '<':
        case '|':
            return 0;
        default:
            return 1;
    }
}


void skip_spaces( char cmd_line[LINESIZE], int * ch_index )
{
    for (; cmd_line[*ch_index] == ' '; (*ch_index)++);
}


void read_command_into( char cmd_line[LINESIZE], char destination[], int * ch_index )
{
    for (int i = 0; is_stopping_char(cmd_line[*ch_index]); (*ch_index)++, i++)
        destination[i] = cmd_line[*ch_index];
}


void read_filename( char cmd_line[LINESIZE], int * ch_index, char fname[50] )
{
    skip_spaces(cmd_line, ch_index);
    read_command_into(cmd_line, fname, ch_index);
}


void redirect_char( char cmd_line[LINESIZE], char stat_arg[100], char infile[50], char outfile[50], char ch, int * pipe_no, int * ch_index, int * arg_no )
{
    switch (ch) {
        case ' ':
        case '\n':
            (*ch_index)++;
            break;
        case '<':
            (*ch_index)++;
            read_filename(cmd_line, ch_index, infile);
            break;
        case '>':
            (*ch_index)++;
            read_filename(cmd_line, ch_index, outfile);
            break;
        case '|':
            (*ch_index)++;
            (*arg_no) = 0;
            (*pipe_no)++;
            break;
        case '&':
            (*ch_index)++;
            should_wait = 0;
            break;
            
        default:
            (*arg_no)++;
            read_command_into(cmd_line, stat_arg, ch_index);
            break;
    }
}


void parse_command( char cmd_line[LINESIZE], char static_args[PIPECAP][ARGCAP][100], char infile[50], char outfile[50], int * pipe_no, int * arg_no, int * ch_index )
{
    skip_spaces(cmd_line, ch_index);
    while (cmd_line[*ch_index] != '\0' )//&& cmd_line[*ch_index] != '|')
    {
        redirect_char( cmd_line, static_args[*pipe_no][*arg_no], infile, outfile, cmd_line[*ch_index], pipe_no, ch_index, arg_no );
    }
}


void open_outfile ( char outfile[100], int *new_ofd )
{
    if ( (*new_ofd = open( outfile, O_CREAT|O_TRUNC|O_WRONLY, 0666 )) < 0)
    {
        perror(outfile);
        exit(1);
    }
}


void open_infile ( char infile[100], int *new_ifd )
{
    if ( (*new_ifd = open( infile, O_RDONLY )) < 0)
    {
        perror(infile);
        exit(1);
    }
}


void redirect_outfile( char outfile[100] )
{
    if ( outfile[0] )
    {
        int new_ofd;
        open_outfile(outfile, &new_ofd);
        dup2(new_ofd, 1);
    }
}


void redirect_infile( char infile[100] )
{
    if ( infile[0] )
    {
        int new_ifd;
        open_infile( infile, &new_ifd);
        dup2(new_ifd, 0);
    }
}


void single_child (char outfile[50], char infile[50] )
{
    redirect_outfile(outfile);
    redirect_infile(infile);
}


void last_child ( char outfile[50] )
{
    redirect_outfile(outfile);
}


void zeroth_child ( char infile[50] )
{
    redirect_infile(infile);
}


void redirect_processes( int pipe_no, int child_no, char infile[50], char outfile[50] )
{
    if ( pipe_no == 0 )  // no pipes
    {
        single_child(outfile, infile);
    }
    else if ( child_no == 0) //first child
    {
        zeroth_child(infile);
    }
    else if ( child_no == pipe_no ) //last child
    {
        last_child(outfile);
    }
}


void args_to_nullterm( char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], int child_no )
{
    for (int i = 0; strcmp(static_args[child_no][i], ""); i++ )
    {
        cmd_args[i] = static_args[child_no][i];
        cmd_args[i+1] = NULL;
    }
}


void do_exec( char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], char infile[50], char outfile[50], int pipe_no, int child_no )
{
    redirect_processes(pipe_no, child_no, infile, outfile);
    args_to_nullterm(static_args, cmd_args, child_no);
    // execvpe(cmd_args[0], cmd_args, envp);
    execvp(cmd_args[0], cmd_args);
    perror("exec");
}


void do_fork( char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], char infile[50], char outfile[50], int pipe_no)
{
    int pip[2];
    int fdin = 0;
    
    for (int child_no = 0; child_no <= pipe_no; child_no++)
    {
        pipe(pip);
        switch (fork())
        {
            case -1:
                perror("Fork");
                break;
            case 0:
                dup2(fdin, 0);
                if (child_no != pipe_no)
                {
                    dup2(pip[1],1);
                }
                close(pip[0]);
                do_exec( static_args, cmd_args, infile, outfile, pipe_no, child_no );
                
            default:
                wait(NULL);
                close(pip[1]);
                fdin = pip[0];
                break;
        }
    }
}


void create_processes ( char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], char infile[50], char outfile[50], int pipe_no )
{
    switch (fork())
    {
        case -1:
            perror("Fork");
            break;
        case 0:
            do_fork( static_args, cmd_args, infile, outfile, pipe_no);
            exit(0);
            
        default:
            if (should_wait)
            {
                just_wait();
            }
            break;
    }
}


void print_prompt()
{
    fflush(stdout);
    putc('~', stdout);
    putc(' ', stdout);
    fflush(stdout);
}


void reset_some_values ( char * cmd_args[50], char infile[50], char outfile[50] )
{
    for (int i = 0;  i < 50 ; i++)
    {
        cmd_args[i] = NULL;
    }
    memset(infile, '\0', 50);
    memset(outfile, '\0', 50);
}


void reset_values( char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], char infile[50], char outfile[50] )
{
    for (int d = 0; d < 10; d++)
    {
        for (int i = 0; i<50; i++)
        {
            memset(static_args[d][i], '\0', 100);
        }
    }
    
    reset_some_values( cmd_args, infile, outfile );
    should_wait = 1;
}


void process_command( char cmd_line[LINESIZE], char static_args[PIPECAP][ARGCAP][100], char * cmd_args[50], char infile[50], char outfile[50] )
{
    int arg_no = 0;
    int pipe_no = 0;
    int index = 0;

    parse_command(cmd_line, static_args, infile, outfile, &pipe_no, &arg_no, &index);
    create_processes(static_args, cmd_args, infile, outfile, pipe_no);
    reset_values(static_args, cmd_args, infile, outfile);
}


int main( int argc, const char * argv[], char * envp[] )
{
    char cmd_line[LINESIZE], static_args[PIPECAP][ARGCAP][100] = {{ "" }};
    char * cmd_args[50];
    char infile[50] = {'\0'}; 
    char outfile[50] = {'\0'};
    
    print_prompt();
    setbuf(stdout, NULL); ///
    while ( fgets(cmd_line, LINESIZE, stdin) != NULL )
    {
        process_command( cmd_line, static_args, cmd_args, infile, outfile );
        print_prompt();
    }
    return 0;
}
