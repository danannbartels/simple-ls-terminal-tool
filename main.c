#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

typedef struct {
  int all;
  int lng;
  int recursive;
} Options;

typedef struct {
  char *name;
  int is_dir;
  long long size;
  time_t mtime;
} Entry;

static int use_color = 0;

// Enable colors only when stdout is a real terminal.
static void setup_color(void) {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode;
  if (GetConsoleMode(h, &mode) &&
      SetConsoleMode(h, mode | 0x0004)) // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    use_color = 1;
#else
  use_color = isatty(STDOUT_FILENO);
#endif
}

static char *dup_str(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (p)
    memcpy(p, s, n);
  return p;
}

static void join_path(char *out, size_t size, const char *dir,
                      const char *name) {
  size_t len = strlen(dir);
  const char *sep =
      (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) ? "" : "/";
  snprintf(out, size, "%s%s%s", dir, sep, name);
}

// Case-insensitive name comparison for qsort.
static int cmp_entries(const void *a, const void *b) {
  const char *x = ((const Entry *)a)->name;
  const char *y = ((const Entry *)b)->name;
  while (*x && *y) {
    int d = tolower((unsigned char)*x) - tolower((unsigned char)*y);
    if (d)
      return d;
    x++;
    y++;
  }
  return tolower((unsigned char)*x) - tolower((unsigned char)*y);
}

static void print_entry(const Entry *e, const Options *opt) {
  if (opt->lng) {
    char when[32] = "?";
    struct tm *tm = localtime(&e->mtime);
    if (tm)
      strftime(when, sizeof when, "%Y-%m-%d %H:%M", tm);
    printf("%c %10lld %s ", e->is_dir ? 'd' : '-', e->size, when);
  }

  if (e->is_dir) {
    if (use_color)
      printf("\x1b[1;34m%s\x1b[0m/\n", e->name);
    else
      printf("%s/\n", e->name);
  } else {
    printf("%s\n", e->name);
  }
}

static int list_dir(const char *path, const Options *opt, int print_header) {
  DIR *dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "cannot open '%s': ", path);
    perror("");
    return -1;
  }

  Entry *entries = NULL;
  size_t count = 0, cap = 0;
  struct dirent *de;

  while ((de = readdir(dir)) != NULL) {
    if (!opt->all && de->d_name[0] == '.')
      continue;

    if (count == cap) {
      size_t new_cap = cap ? cap * 2 : 32;
      Entry *tmp = realloc(entries, new_cap * sizeof *entries);
      if (!tmp) {
        fprintf(stderr, "out of memory\n");
        break;
      }
      entries = tmp;
      cap = new_cap;
    }

    char full[4096];
    join_path(full, sizeof full, path, de->d_name);

    struct stat st;
    Entry *e = &entries[count];
    e->name = dup_str(de->d_name);
    if (!e->name)
      break;
    if (stat(full, &st) == 0) {
      e->is_dir = S_ISDIR(st.st_mode);
      e->size = (long long)st.st_size;
      e->mtime = st.st_mtime;
    } else {
      e->is_dir = 0;
      e->size = 0;
      e->mtime = 0;
    }
    count++;
  }
  closedir(dir);

  qsort(entries, count, sizeof *entries, cmp_entries);

  if (print_header)
    printf("%s:\n", path);
  for (size_t i = 0; i < count; i++)
    print_entry(&entries[i], opt);

  if (opt->recursive) {
    for (size_t i = 0; i < count; i++) {
      const char *n = entries[i].name;
      if (!entries[i].is_dir || strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
        continue;
      char sub[4096];
      join_path(sub, sizeof sub, path, n);
      printf("\n");
      list_dir(sub, opt, 1);
    }
  }

  for (size_t i = 0; i < count; i++)
    free(entries[i].name);
  free(entries);
  return 0;
}

int main(int argc, char *argv[]) {
  Options opt = {0, 0, 0};
  const char *path = ".";

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      // Supports combined flags like -al or -lR
      for (const char *f = argv[i] + 1; *f; f++) {
        switch (*f) {
        case 'a':
          opt.all = 1;
          break;
        case 'l':
          opt.lng = 1;
          break;
        case 'R':
          opt.recursive = 1;
          break;
        default:
          fprintf(stderr, "Usage: %s [-alR] [path]\n", argv[0]);
          return 1;
        }
      }
    } else {
      path = argv[i];
    }
  }

  setup_color();
  return list_dir(path, &opt, opt.recursive) == 0 ? 0 : 1;
}
