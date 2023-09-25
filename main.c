#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

size_t get_hash_from_bytes(char *bytes, size_t byte_count) {
  size_t hash = 5381;
  if (!bytes) { return 0; }
  for (size_t i = 0; i < byte_count; ++i) {
    hash = hash * 33 + *bytes;
    bytes++;
  }
  return hash;
}

size_t get_hash_from_string_passthrough(char *string, size_t hash) {
  if (!string) { return hash; }
  while (*string) {
    hash = hash * 33 + *string;
    string++;
  }
  return hash;
}

// POSITIVE NUMBER LARGER THAN ZERO.
#define MARKOV_CONTEXT_SIZE 4

typedef struct MarkovContext {
  char *previous_words[MARKOV_CONTEXT_SIZE];
} MarkovContext;

void print_context(MarkovContext context) {
  if (context.previous_words[0]) {
    printf("[\"%s\"", context.previous_words[0]);
  } else {
    printf("[NULL");
  }
  for (size_t i = 1; i < MARKOV_CONTEXT_SIZE; ++i) {
    if (context.previous_words[i]) {
      printf(", \"%s\"", context.previous_words[i]);
    } else {
      printf(", NULL");
    }
  }
  putchar(']');
}

/// @return Zero iff both contexts point to matching strings.
int compare_contexts(MarkovContext a, MarkovContext b) {
  for (size_t i = 0; i < MARKOV_CONTEXT_SIZE; ++i) {
    if (a.previous_words[i] == NULL && b.previous_words[i] == NULL) {
      continue;
    }
    if (a.previous_words[i] == NULL || b.previous_words[i] == NULL) {
      return 1;
    }
    if (strcmp(a.previous_words[i], b.previous_words[i]) != 0) {
      //printf("\"%s\" != \"%s\"\n", a.previous_words[i], b.previous_words[i]);
      return 1;
    }
  }
  //print_context(a);
  //printf(" = ");
  //print_context(b);
  //printf("\n");
  return 0;
}

size_t get_hash_from_context(MarkovContext context) {
  size_t hash = 5381;
  for (size_t i = 0; i < MARKOV_CONTEXT_SIZE; ++i) {
    hash = get_hash_from_string_passthrough(context.previous_words[i], hash);
  }
  return hash;
}

MarkovContext markov_context_copy(MarkovContext original) {
  MarkovContext context;
  for (size_t i = 0; i < MARKOV_CONTEXT_SIZE; ++i) {
    context.previous_words[i] = strdup(original.previous_words[i]);
  }
  return context;
}

typedef struct MarkovValue {
  char *word;
  size_t accumulator;
  size_t total_occurrences;  // field to keep track of total occurrences
  struct MarkovValue *next;
} MarkovValue;


MarkovValue *markov_value_create(MarkovValue *parent, char *word) {
  MarkovValue *value = malloc(sizeof(MarkovValue));
  value->word = word;
  value->accumulator = 1;
  value->total_occurrences = 1;  // Initialize total_occurrences
  value->next = parent;
  return value;
}

void print_value(MarkovValue *value) {
  if (!value) { return; }
  printf("[%s, %zu]", value->word, value->accumulator);
  value = value->next;
  while (value) {
    printf(", [%s, %zu]", value->word, value->accumulator);
    value = value->next;
  }
}

typedef struct SandCastle {
  MarkovContext context;
  // Value is never NULL in a used sand_castle.
  MarkovValue *value;
  struct SandCastle *next;
} SandCastle;

typedef struct MarkovModel {
  SandCastle *sand_castles;
  size_t capacity;
} MarkovModel;

MarkovModel markov_model_create(size_t count) {
  MarkovModel model;
  model.capacity = count;
  model.sand_castles = calloc(count, sizeof(SandCastle));
  assert(model.sand_castles && "Could not allocate memory for hash map sand_castles array.");
  return model;
}

void markov_model_add_word(MarkovModel model, MarkovContext context, char *word) {
  if (!word) { return; }
  size_t hash = get_hash_from_context(context);
  size_t index = hash % model.capacity;
  SandCastle *sand_castle = &model.sand_castles[index];
  SandCastle *start_sand_castle = sand_castle;
  //printf("SandCastle index: %zu\n", index);
  //printf("Context: "); print_context(context); printf("\r\n");
  // [ [ ["","",""] -> ["", N], ["", N1], ["", N2], ["", N3] ], [ ["","",""] -> ["", N4], ["", N5] ] ]
  if (sand_castle->value == NULL) {
    // SandCastle is empty, never had anything mapped within it.
    sand_castle->context = markov_context_copy(context);
    sand_castle->value = markov_value_create(NULL, word);
    sand_castle->next = NULL;
    return;
  } else {
    do {
      if (compare_contexts(sand_castle->context, context) == 0) {
        //printf("Contexts match\n");
        MarkovValue *value = sand_castle->value;
        while (value) {
          if (strcmp(word, value->word) == 0) {
            value->accumulator++;
            value->total_occurrences++;  // Update total_occurrences
          return;
          }
          value = value->next;
        } 
        //printf("New word mapping\n");
        // Contexts match, but word hasn't been mapped yet.
        // Create new mapping in value linked list.
        // FIXME: This may be less efficient as it adds new words
        // to the beginning of the linked list, and those are most
        // likely not the most often visited. Sort the list!
        sand_castle->value = markov_value_create(sand_castle->value, word);
        return;
      }
      sand_castle = sand_castle->next;
    } while (sand_castle);
    //printf("Contexts don't match\n");
    // Contexts do not match, create new sand_castle to prevent collision.
    // START_SAND_CASTLE     -> NULL
    // NEW_SAND_CASTLE       -> NULL
    // START_SAND_CASTLE     -> NEW_SAND_CASTLE         -> NULL
    SandCastle *new_sand_castle = malloc(sizeof(SandCastle));
    assert(new_sand_castle && "Could not allocate single sand_castle for hash map sand_castle linked list.");
    new_sand_castle->context = markov_context_copy(context);
    new_sand_castle->value = markov_value_create(NULL, word);
    new_sand_castle->next = start_sand_castle->next;
    start_sand_castle->next = new_sand_castle;
    return;
  }
}

void markov_model_free(MarkovModel model) {
  // TODO: Free each sand_castle's value's word
  //       Free each sand_castle's values
  //       Free each sand_castle's context's previous words.
  //       Free model.sand_castles
}

void markov_model_print(MarkovModel model) {
  for (size_t i = 0; i < model.capacity; ++i) {
    SandCastle *sand_castle = &model.sand_castles[i];
    if (sand_castle->value != NULL) {
      print_context(sand_castle->context);
      printf(" -> ");
      print_value(sand_castle->value);
      sand_castle = sand_castle->next;
      while (sand_castle) {
        printf(", ");
        print_context(sand_castle->context);
        printf(" -> ");
        print_value(sand_castle->value);
        sand_castle = sand_castle->next;
      }
      putchar('\n');
    }
  }
}

// Assumes that offset + length is within the given string.
char *allocate_string_span(char *string, size_t offset, size_t length) {
  if (!string || length == 0) { return NULL; }
  char *out = malloc(length + 1);
  assert(out && "Could not allocate span from string.");
  memcpy(out, string + offset, length);
  out[length] = '\0';
  return out;
}

void markov_model_train_string_space_separated(MarkovModel model, char *string) {
  MarkovContext context;
  memset(&context, 0, sizeof(MarkovContext));
  const char *string_it        = string;
  const char *start_of_word    = string;
  const char *end_of_word      = string;
  const char *const whitespace = " \r\n\f\e\v\t";
  while (string_it && *string_it) {
    // Skip whitespace at the beginning of the string.
    string_it += strspn(string_it, whitespace);
    start_of_word = string_it;

    // Skip until whitespace (end of word).
    string_it = strpbrk(string_it, whitespace);
    if (!string_it) { break; }
    end_of_word = string_it;

    // Map context to word in markov model.
    size_t offset = start_of_word - string;
    size_t length = end_of_word - start_of_word;
    char *word = allocate_string_span(string, offset, length);
    //printf("Got word: \"%s\"\n", word);
    markov_model_add_word(model, context, word);
    //markov_model_print(model);

    // Update context.
    if(context.previous_words[0]) {
      free(context.previous_words[0]);
      context.previous_words[0] = NULL;
    }
    for (int i = 1; i < MARKOV_CONTEXT_SIZE; ++i) {
      context.previous_words[i - 1] = context.previous_words[i];
    }
    context.previous_words[MARKOV_CONTEXT_SIZE - 1] = strdup(word);
  }
  if (start_of_word != string_it) {
    size_t offset = start_of_word - string;
    while (*end_of_word) { end_of_word++; }
    size_t length = end_of_word - start_of_word;
    char *word = allocate_string_span(string, offset, length);
    markov_model_add_word(model, context, word);
  }
  for (size_t i = 0; i < MARKOV_CONTEXT_SIZE; ++i) {
    if (context.previous_words[i]) {
      free(context.previous_words[i]);
    }
  }
}

int get_file_size(FILE *file) {
  if (!file) { return -1; }
  fpos_t original = 0;
  int status = 0;
  status = fgetpos(file, &original);
  if (status) { return -2; }
  status = fseek(file, 0, SEEK_END);
  if (status) { return -3; }
  size_t file_size = ftell(file);
  fsetpos(file, &original);
  return file_size;
}

/// @return Zero upon success.
int markov_model_train_from_file(MarkovModel model, char *filepath) {
  // TODO: Open file, create buffer to read contents, etc.
  //       Read into buffer, use buffer as string

  FILE *file = fopen(filepath, "rb");
  if (!file) { return 1; }

  const size_t buffer_size = 4096;
  char *buffer = malloc(buffer_size);
  assert(buffer && "Could not allocate memory for temporary file buffer.");

  int bytes_left = get_file_size(file);
  if (bytes_left <= 0) { return 2; }
  while (bytes_left) {
    // TODO: Seek to start of newline before reading buffer, or something like that.
    size_t bytes_read = fread(buffer, 1, buffer_size - 1, file);
    buffer[bytes_read] = '\0';
    //printf("Got file chunk:\n---\n%s---\n", buffer);
    char *temp_buffer = buffer + strcspn(buffer, "\n") + 1;
    markov_model_train_string_space_separated(model, temp_buffer);
    bytes_left -= bytes_read;
  }

  return 0;
}


SandCastle *markov_model_lookup(MarkovModel model, MarkovContext context) {
  size_t hash = get_hash_from_context(context);
  size_t index = hash % model.capacity;
  return &model.sand_castles[index];
}

char *get_random_word_from_value(MarkovValue *value) {
  if (!value) { return NULL; }
  MarkovValue *it = value;
  size_t total = 0;
  while (it) {
     total += (it->accumulator + 1);  // Add 1 for Laplace smoothing
    it = it->next;
  }
  int random = rand() % total;
  while (value) {
      random -= (value->accumulator + 1);  // Add 1 for Laplace smoothing
    if (random <= 0) {
      return value->word;
    }
    value = value->next;
  }
  return NULL;
}

char *generate_text(MarkovModel model, size_t text_length) {
  const char *separator = " ";
  size_t sep_length = strlen(separator);

  char *buffer = malloc(text_length + 1);
  assert(buffer && "Could not allocate buffer for text generation.");
  memset(buffer, 0, text_length + 1);
  char *buffer_it = buffer;

  // Get bucket from context.
  MarkovContext context;
  memset(&context, 0, sizeof(MarkovContext));

  int bytes_to_write = text_length;
  while (bytes_to_write > 0) {
    SandCastle *sand_castle = markov_model_lookup(model, context);
    char *word_to_write = NULL;
    while (sand_castle) {
      if (compare_contexts(sand_castle->context, context) == 0) {
        // TODO: Determine which value to use based on accumulators and random number.
        word_to_write = get_random_word_from_value(sand_castle->value);
        break;
      }
      sand_castle = sand_castle->next;
    }

    //printf("Generated word: \"%s\"\n", word_to_write);
    //printf("From context: ");
    //print_context(context);
    //putchar('\n');

    // Update context.
    for (int i = 1; i < MARKOV_CONTEXT_SIZE; ++i) {
      context.previous_words[i - 1] = context.previous_words[i];
    }
    context.previous_words[MARKOV_CONTEXT_SIZE - 1] = word_to_write;

    if (!word_to_write) { continue; }

    size_t word_length = strlen(word_to_write);
    if (buffer_it + word_length + sep_length >= buffer + text_length) {
      *buffer_it = '\0';
      break;
    }
    memcpy(buffer_it, word_to_write, word_length);
    buffer_it += word_length;
    bytes_to_write -= word_length;
    memcpy(buffer_it, separator, sep_length);
    buffer_it += sep_length;
    bytes_to_write -= sep_length;
  }
  buffer[text_length] = '\0';
  return buffer;
}

int main(int argc, char **argv) {
  srand(time(NULL));

  if (argc != 2) {
    printf("USAGE: `%s your_training_data_here.txt`", argv[0]);
    return 0;
  }

  // FIXME: When this number is small, it seems that a bug in the
  // linked list code comes out, as the program crashes on Windows
  // with ACCESS_DENIED exit code `5`.
  // I think this is due to hash map collision prevention code.
  MarkovModel model = markov_model_create(100000);

  markov_model_train_from_file(model, argv[1]);

  //markov_model_print(model);

  char *generated_text = generate_text(model, 4096);
  printf("Generated text:\n---\n%s\n---\n", generated_text);
  free(generated_text);

  FILE *outfile = fopen("first_ever_generated_text.txt", "wb");
  assert(outfile && "Could not open output file for writing.");
  for (size_t i = 0; i < 2048; ++i) {
    const size_t size = 4096;
    char *tmp_text = generate_text(model, size);
    fwrite(tmp_text, 1, size, outfile);
    fwrite("\n", 1, 1, outfile);
    free(tmp_text);
  }

  markov_model_free(model);

  printf("COMPLETE\n");

  return 0;
}
