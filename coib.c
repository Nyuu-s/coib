

/*
 * TODO:
 * move things to header file
 *
 */

#include <sys/types.h>
#define PROJECT_ROOT "/home/nyuu/Documents/git/coib/"
// TODO MOVE TO HEADER FILE////////////////////
#ifndef PROJECT_ROOT
#error "YOU NEED TO DEFINE PROJECT_ROOT!"
#endif
///////////////////////////////////////////////
#ifndef COIB_HEADER
#define COIB_HEADER "coib.h"
#endif
#include COIB_HEADER



#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SUBSTR_SCRATCH_SIZE 2048
#define CFLAGS "-Wall -g -O2"


// temp: set both to 0 to use default path
#define MANIFEST_FLAG 0
#define DB_FLAG 0

#define ARR_SIZE(array) ((sizeof(array))/(sizeof(array[0])))
#define DEBUG_STR_ARRAY(array) do { \
for(int i = 0; i < ARR_SIZE(array); i++) printf("%d/%d: %s\n", i+1,ARR_SIZE(array), array[i]);\
} while(0);

#define CMD_APPEND(cmd, elem, ...) do { \
    char *arr[] = { elem, __VA_ARGS__ }; \
    cmd_split_and_append(cmd, arr, ARR_SIZE(arr)); \
} while (0);

#define ALIGN_UP_U8(p,alignment)  (uint8_t*) (((uintptr_t)(p) + ((alignment)-1)) & ~((alignment)-1));
#define ALIGN8(p) ALIGN_UP_U8(p,8)
#define ALIGN16(p) ALIGN_UP_U8(p,16)


typedef unsigned short uint16; 
typedef struct {
  uint8_t* base;
  uint8_t* end;
  uint8_t* offset;
  size_t capacity;
}Arena;

typedef struct {
  char** data;
  uint16 count;
  uint16 capacity;
  Arena* arena;
} Command;




void arena_init(Arena* arena, size_t cap){
  if(arena == NULL)
  {
    if(arena == NULL) return;
  }
  if(cap <= 0) return;
  
  arena->base = (uint8_t*) malloc(cap * sizeof(*arena->base));
  if(arena->base==NULL) return; 
  arena->capacity = cap;
  arena->end = arena->base + arena->capacity; 
  arena->offset = arena->base;
}

uint8_t* arena_alloc(Arena* arena,  size_t uint8_size){
  if(arena == NULL || uint8_size <= 0) return NULL;
  
  uint8_t* aligned = ALIGN8(arena->offset);
  uint8_t* next = aligned + uint8_size;

  if(next > arena->end) {printf("Arena is full !"); return NULL;}
  arena->offset = next;
  return aligned;

}

void arena_reset(Arena* arena)
{
  arena->offset = arena->base;
}

void arena_free(Arena* arena){
  free(arena->base);
}

void _cmd_push(Command* cmd, char* element)
{
    if(cmd->count >= cmd->capacity)
    {
        cmd->capacity = cmd->capacity == 0? 256: cmd->capacity *2;

        char** newdata = realloc(cmd->data, sizeof(*cmd->data) * cmd->capacity);
        if(!newdata){ printf("Error failed to realocate cmd"); return;}
        cmd->data = newdata; 
    }
    cmd->data[cmd->count++] = element; 
}


void cmd_split_and_append(Command* cmd, char** elements, size_t n)
{
  if(cmd->arena == 0){
    cmd->arena = (Arena*) malloc(sizeof(Arena));
    if(cmd->arena == 0){ 
        printf("Failed to initiate pointer to substring scratch arena\n");
    }
    arena_init(cmd->arena, SUBSTR_SCRATCH_SIZE);
    if(cmd->arena->base == NULL){
        printf("Failed to initiate substring scratch arena\n");
        return;
      }
    }
   uint8_t is_quote = 0;
   for(int i=0;i <n; ++i){
     uint32_t len = strlen(elements[i]);
     size_t k=0;
     for(int j = 0; j < len+1; j++){
       if(!is_quote && elements[i][j] == '"'){
          is_quote = 1;
       }else if (is_quote && elements[i][j] == '"') {
          is_quote = 0;
       }
        if(!is_quote && (elements[i][j] == ' ' || elements[i][j] == 0)){
          
          uint8_t* r = arena_alloc(cmd->arena, (j-k));
          if(!r) {
            printf("failed allocation of string in arena"); 
            return;
          }
          memcpy(r, elements[i]+k, (j-k));
          r[j-k] = '\0';
          _cmd_push(cmd, (char*)r);
          k = j+1;
        }
        
     }
   }
}


typedef struct SizedString {
  char* value;
  size_t length;
}SizedString;

typedef struct SourceDir  {
  SizedString path;
  SizedString ext;
}SourceDir;

int cmd_run(Command* cmd){

  pid_t pid = fork();
  if(pid < 0) exit(1);
  else if (pid == 0){
    printf("Executing: ");
    for (int i=0; i<cmd->count; i++) {
      printf("%s ", cmd->data[i]);
    }
    printf("\n");
    execvp(cmd->data[0],cmd->data);
    perror("failed execvp");
    exit(1);
  }
  else{
  int status;
  waitpid(pid, &status, 0);
  if(WIFEXITED(status) && WEXITSTATUS(status) > 0) printf("Child process terminated with status: %d\n", WEXITSTATUS(status));
}

  arena_reset(cmd->arena);
  cmd->count = 0;
  return 1;
}

int check_need_rebuild(const char* binary_path, const char* inputs[], size_t inputs_size){
    char path_buf[1024];
    snprintf(path_buf, strlen(PROJECT_ROOT)+1, "%s", PROJECT_ROOT);
    if(MANIFEST_FLAG){
      return 0;
      // track files using, a manifest of timestamps/hashed content
    }
    if(DB_FLAG){
      return 0;
      // embed sqllite and use a local db to store inputs metadata and builds metadata, used when more complex build required with incremental or conditionals builds 
    }
    
    // default, just check mtime and file size
    strcat(path_buf, binary_path);
    printf("%s\n", path_buf);
    struct stat filestat;
    struct stat binstat;
    if(stat(path_buf, &binstat) != 0){
      //binary not found, rebuild
      printf("Binary not found, triggering target's commands...\n");
      return 1;

    }
    for(int i=0; i< inputs_size; ++i){
      if(stat(inputs[i], &filestat) == 0 && filestat.st_mtime > binstat.st_mtime){
          printf("Inputs changed, rebuilding...\n");
          return 1;
      }
    }

    printf("No changes detected, skipping rebuild.\n");
    return 0;
}

typedef struct{
  const char** inputs;
  size_t input_count;
  const char* output;
  Command* commands;
  size_t command_count;
  size_t command_capacity;
} Target;

typedef struct {
  Target** items;
  size_t size;
  size_t capacity;
}Targets;


#define CREATE_SELF_TARGET(arena, binary) \
  do {\
   const char* sources[] = {\
    __FILE__,\
    COIB_HEADER,\
  };\
  Target* self = INIT_TARGET((arena), ARR_SIZE(sources), (binary), 1);\
  } while(0);

Target* init_target(Arena* arena, const char** sources, size_t source_count, size_t command_capacity, const char* binary_path){
  Target* t = (Target*) arena_alloc(arena, sizeof(Target));
  t->command_count = 0;
  t->command_capacity = command_capacity;
  t->input_count = source_count;
  t->inputs = sources;
  t->output = binary_path;
  t->commands = (Command*) arena_alloc(arena, command_capacity * sizeof(Command));
  
  return t;
}

Command* target_new_command_trigger(Target* target){
  if(target->command_count >= target->command_capacity) {
    printf("Target has no more room for commands trigger! %lu >= %lu\n", target->command_count, target->command_capacity);
    return NULL; }
  return &target->commands[target->command_count++];
}
#define CHECK_SELF_REBUILD(binary) \
  do {\
    if(check_need_rebuild((binary),(const char*[]){__FILE__, COIB_HEADER}, 2)){\
      Command c = {0};\
      CMD_APPEND(&c,"gcc");\
      CMD_APPEND(&c,CFLAGS);\
      CMD_APPEND(&c, __FILE__);\
      CMD_APPEND(&c,(binary));\
      cmd_run(&c);\
  }\
  }while(0);

int main(int argc, char* argv[]){

  // CHECK_SELF_REBUILD(argv[0]);
 

  if(1){
    //METHOD 1
  Targets targets = {0};

  Arena arena;
  arena_init(&arena, 2048);

  const char* in = {"hello.c"};
  Target* t1 = init_target(&arena, &in, 1, 2, "hello");
  targets.items = (Target**) arena_alloc(&arena, sizeof(Target*) * 1);
  targets.items[0] = t1;
  targets.capacity = 1;
  targets.size = 1;
  

  Command* cmd1 = target_new_command_trigger(t1);
  Command* cmd2 = target_new_command_trigger(t1);
  
  CMD_APPEND(cmd1, "echo \"");
  CMD_APPEND(cmd1, "lets have fun !\"");

  CMD_APPEND(cmd2, "gcc");
  CMD_APPEND(cmd2, CFLAGS);
  CMD_APPEND(cmd2, "hello.c");
  CMD_APPEND(cmd2, "-o hello");  
  // CHECK_TARGETS_REBUILD(&targets);
  
  for (int i=0;i<targets.size; ++i) {
    Target* t = targets.items[i];
    if(check_need_rebuild(t->output,t->inputs , t->input_count)){
      for (int k = 0; k<t->command_count; ++k) {
          cmd_run(&t->commands[k]);
      }
    }
  }

  Command cmd = {0};
  CMD_APPEND(&cmd, "./hello");
  cmd_run(&cmd);


  } else {
    //METHOD 2
    Command cmd = {0};
    CMD_APPEND(&cmd, "gcc");
    CMD_APPEND(&cmd, CFLAGS);
    CMD_APPEND(&cmd, "coib.c");
    CMD_APPEND(&cmd, "-o coib_test3");
  
    cmd_run(&cmd); // runs an clear cmd;
                 
    CMD_APPEND(&cmd, "gcc");
    CMD_APPEND(&cmd, CFLAGS);
    CMD_APPEND(&cmd, "coib.c");
    CMD_APPEND(&cmd, "-o coib_test4");
  
    cmd_run(&cmd);
  }
    return 0;
}
