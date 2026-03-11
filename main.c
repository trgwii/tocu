#define MAX_FILE_SIZE (16 * 1024 * 1024)
#define MAX_HEADINGS (32 * 1024)

// Base requirements
#include <stdio.h> // FILE, fopen, ferror, fread, fwrite, fprintf, fclose, stdout
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS, size_t

typedef unsigned char u8;

typedef struct {
  char *data;
  size_t size;
} Str;

static const Str emptyStr = {.data = NULL, .size = 0};

static size_t len(const char *const s) {
  size_t i = 0;
  while (s[i] != '\0')
    i++;
  return i;
}

static u8 streq(const char *const a, const char *const b, const size_t n) {
  if (a == b)
    return 1;
  if (a == NULL || b == NULL)
    return 0;
  if (n == 0)
    return 1;
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i])
      return 0;
  return 1;
}

static u8 cstreq(const char *const a, const char *const b) {
  size_t a_len = len(a);
  size_t b_len = len(b);
  if (a_len != b_len)
    return 0;
  return streq(a, b, a_len);
}

static char file_data[MAX_FILE_SIZE];

static Str readEntireFile(const char *const path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return emptyStr;
  size_t bytes_read = fread(file_data, 1, MAX_FILE_SIZE, f);
  if (bytes_read < MAX_FILE_SIZE) {
    if (ferror(f)) {
      // Failed to read, abort
      fclose(f);
      return emptyStr;
    }
    // EOF, only successful return path
    fclose(f);
    // zero-terminate
    file_data[bytes_read] = '\0';

    return (Str){.data = file_data, .size = bytes_read};
  }
  // bytes_read == bytes_to_read
  // actual file size must be > MAX_FILE_SIZE, abort
  fclose(f);
  return emptyStr;
}

typedef struct {
  Str text;
  size_t offset;
} LineIterator;

static Str nextLine(LineIterator *it) {
  size_t start = it->offset;
  if (start == it->text.size)
    return emptyStr;
  size_t off = start;
  while (off != it->text.size && it->text.data[off] != '\n')
    off++;
  size_t size = off - it->offset;
  it->offset = off + (off != it->text.size);
  return (Str){.data = it->text.data + start, .size = size};
}

typedef struct {
  u8 level;
  Str text;
} Heading;

static void write_slug(FILE *out, Str string, unsigned duplicate) {
  char buf[128];
  size_t off = 0;
  for (size_t i = 0; i < string.size; i++) {
    char c = string.data[i];
    if (c == '.' || c == ',' || c == '(' || c == ')' || c == '#' || c == '`' ||
        c == '/')
      continue;
    if (c == ' ') {
      if (off == sizeof buf) { // flush
        fwrite(buf, off, 1, out);
        off = 0;
      }
      buf[off++] = '-';
      continue;
    }
    if (c >= 'A' && c <= 'Z')
      c += 32;
    if (off == sizeof buf) { // flush
      fwrite(buf, off, 1, out);
      off = 0;
    }
    buf[off++] = c;
  }
  // final flush
  fwrite(buf, off, 1, out);
  if (duplicate)
    fprintf(out, "-%u", duplicate);
}

static Heading headings[MAX_HEADINGS];

#define HELP_TEXT                                                              \
  ("Formats a Table of Contents section within a README.md file according to " \
   "the headings inside the markdown file\n"                                   \
   "Usage: `tocu <args>`\n"                                                    \
   "Args:\n"                                                                   \
   "\t-h, --help        Show this text and exit\n"                             \
   "\t-s, --silent      Don't output information on the detected headings "    \
   "during a normal run\n"                                                     \
   "\t-b, --bullet      Use bulleted markdown lists instead of numeric\n"      \
   "\t--skip-toc        Don't include the table of contents heading inside "   \
   "itself\n"                                                                  \
   "\t--min-level <1-6> Set the minimum heading level to include in the "      \
   "table of contents. This can be set to 2 to not include the toplevel "      \
   "heading / document title.\n"                                               \
   "")

int main(int argc, char **argv) {
  (void)argc;
  u8 help = 0;
  u8 silent = 0;
  u8 bullet = 0;
  u8 minimum_level = 1;
  u8 skip_toc = 0;
  const char *filename = NULL;
  for (int i = 1; i < argc; i++) {
    if (cstreq(argv[i], "-h") || cstreq(argv[i], "--help"))
      help = 1;
    else if (cstreq(argv[i], "-s") || cstreq(argv[i], "--silent"))
      silent = 1;
    else if (cstreq(argv[i], "-b") || cstreq(argv[i], "--bullet"))
      bullet = 1;
    else if (cstreq(argv[i], "--skip-toc"))
      skip_toc = 1;
    else if (i + 1 < argc && cstreq(argv[i], "--min-level")) {
      char parsed_min_level = *argv[i + 1] - '0';
      if (1 <= parsed_min_level && parsed_min_level <= 6) {
        minimum_level = (u8)parsed_min_level;
        i++;
      }
    } else
      filename = argv[i];
  }
  if (help) {
    fprintf(stdout, HELP_TEXT);
    return EXIT_FAILURE;
  }
  if (!filename)
    filename = "README.md";
  Str readme = readEntireFile(filename);
  if (!readme.data)
    return EXIT_FAILURE;
  size_t headings_count = 0;
  size_t toc_index = 0xFFFFFFFFFFFFFFFF;
  LineIterator it = {.text = readme};
  for (Str line = nextLine(&it); line.data; line = nextLine(&it)) {
    u8 level = 0;
    while (line.size > level && line.data[level] == '#')
      level++;
    if (level > 0 && level <= 6 && line.size > level + 2u &&
        line.data[level] == ' ') {

      // If too many headings in the file; abort
      if (headings_count + 1 > MAX_HEADINGS)
        return EXIT_FAILURE;

      Str heading_text = (Str){
          .data = line.data + level + 1,
          .size = line.size - (level + 1),
      };

      if (toc_index > headings_count &&
          ((heading_text.size == 3 &&
            streq("TOC", heading_text.data, heading_text.size)) ||
           (heading_text.size == 17 &&
            streq("Table of Contents", heading_text.data, heading_text.size)) ||
           (heading_text.size == 8 &&
            streq("Contents", heading_text.data, heading_text.size))))
        toc_index = headings_count;

      headings[headings_count++] = (Heading){level, heading_text};
    }
  }

  if (!silent)
    for (size_t i = 0; i < headings_count; i++)
      fprintf(stdout, "%u%s:%*s %.*s\n", headings[i].level,
              i == toc_index ? " [TOC]" : "      ", (headings[i].level - 1) * 4,
              "", (int)headings[i].text.size, headings[i].text.data);

  // No index found for Table of Contents location, abort
  if (toc_index > headings_count)
    return EXIT_FAILURE;

  FILE *readme_write = fopen(filename, "w");
  if (!readme_write)
    return EXIT_FAILURE;

  { // TODO: error handling
    fwrite(readme.data, 1,
           (size_t)headings[toc_index].text.data +
               headings[toc_index].text.size - (size_t)readme.data,
           readme_write);
    fwrite("\n\n", 1, 2, readme_write);
    for (size_t i = 0; i < headings_count; i++) {
      // Skip Table of Contents?
      if (skip_toc && i == toc_index)
        continue;
      if (headings[i].level < minimum_level)
        continue;
      fprintf(readme_write, "%.*s%s [%.*s](#",
              (int)((headings[i].level - minimum_level) * (bullet ? 2 : 3)),
              "                  ", bullet ? "-" : "1.",
              (int)headings[i].text.size, headings[i].text.data);
      unsigned duplicate = 0;
      for (size_t j = 0; j < i; j++)
        if (headings[j].text.size == headings[i].text.size &&
            streq(headings[j].text.data, headings[i].text.data,
                  headings[i].text.size))
          duplicate++;
      write_slug(readme_write, headings[i].text, duplicate);
      fwrite(")\n", 1, 2, readme_write);
    }
    fwrite("\n", 1, 1, readme_write);
    char *rest_data =
        headings[toc_index + 1].text.data - headings[toc_index + 1].level - 1;
    size_t rest_size = (size_t)readme.data + readme.size - (size_t)rest_data;
    fwrite(rest_data, 1, rest_size, readme_write);

    fclose(readme_write);
  }

  return EXIT_SUCCESS;
}
