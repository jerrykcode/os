#ifndef BUILDIN_CMD_H__
#define BUILDIN_CMD_H__

void make_clear_abs_path(const char *original_path, char *final_path);
void buildin_pwd(int argc, char *argv[]);
void buildin_ps(int argc, char *argv[]);
void buildin_clear(int argc, char *argv[]);
void buildin_cd(int argc, char *argv[]);
void buildin_mkdir(int argc, char *argv[]);
void buildin_rm(int argc, char *argv[]);
void buildin_ls(int argc, char *argv[]);

#endif
