#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <stdint.h>

struct TrunkString {
    uintptr_t addr;
    const char *str;
};

struct TrunkStrings {
    struct TrunkString* sorted_items;
    size_t count;
};

static const uint8_t *find_trunk_section(const uint8_t *elf_data, uint32_t *out_addr, uint32_t *out_size) {
    const Elf32_Ehdr *ehdr = (const void*)elf_data;
    const Elf32_Shdr *shdrs = (const Elf32_Shdr *)(elf_data + ehdr->e_shoff);
    const char *shstrtab = (const char *)(elf_data + shdrs[ehdr->e_shstrndx].sh_offset);

    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        if (strcmp(shstrtab + shdrs[i].sh_name, ".trunk") == 0) {
            *out_addr = shdrs[i].sh_addr;
            *out_size = shdrs[i].sh_size;
            return elf_data + shdrs[i].sh_offset;
        }
    }
    return NULL;
}

static void print_escaped(FILE *out, const char *str) {
    for (; *str != '\0'; ++str) {
        if (*str < 0x1F) {
            switch (*str) {
            case '\n': fprintf(out, "\\n"); break;
            case '\r': fprintf(out, "\\r"); break;
            default: fprintf(out, "\\x%02hhx", *str); break;
            }
        } else {
            fputc(*str, out);
        }
    }
}

static struct TrunkStrings extract_strings(const uint8_t *blob, size_t addr, size_t size) {
    size_t capacity = 8;
    struct TrunkStrings strings;
    strings.sorted_items = calloc(capacity, sizeof(struct TrunkString));
    strings.count = 0;

    const uint8_t *str_end;
    while ((str_end = memchr(blob, '\0', size)) != NULL) {
        if (strings.count >= capacity) {
            capacity *= 2;
            strings.sorted_items = realloc(strings.sorted_items, capacity*sizeof(struct TrunkString));
        }

        size_t str_len = strlen((const char *)blob);
        if (str_len > 0) {
            strings.sorted_items[strings.count++] = (struct TrunkString){
                .addr = addr,
                .str = strdup((const char *)blob)
            };
            fprintf(stderr, "Found string at addr %04lx: `", addr);
            print_escaped(stderr, (const char *)blob);
            fprintf(stderr, "` \n");
        }

        str_end++;
        addr += str_end - blob;
        size -= str_end - blob;
        blob = str_end;
    }

    return strings;
}

static const char *try_find_string(struct TrunkStrings strs, uint32_t ptr) {
    // Classic binary search
    size_t left = 0;
    size_t right = strs.count;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        const uint32_t addr = strs.sorted_items[mid].addr;
        if (addr == ptr) {
            return strs.sorted_items[mid].str;
        } else if (addr < ptr) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return NULL;
}

#define TRUNK_ESCAPE_SYMBOL 0x42
#define TRUNK_START_OF_FRAME 0x00

static struct BufferedReader rd;

struct BufferedReader {
    uint8_t buf[1024];

    uint8_t *begin;
    uint8_t *end;
};

static int read_u32(uint32_t *out) {
    if (rd.begin + 4 <= rd.end) {
        *out = ((rd.begin)[0] << 0) | ((rd.begin)[1] << 8) | ((rd.begin)[2] << 16) | ((rd.begin)[3] << 24);
        rd.begin += sizeof(uint32_t);
        return 1;
    }

    const size_t avail = rd.end - rd.begin;
    memmove(rd.buf, rd.begin, avail);

    ssize_t len;
    size_t total = avail;
    while ((len = read(STDIN_FILENO, rd.buf + total, sizeof(rd.buf) - total)) > 0) {
        total += len;
        if (total >= sizeof(uint32_t)) break;
    }
    rd.begin = rd.buf;
    rd.end = rd.buf + total;

    if (total >= sizeof(uint32_t)) {
        *out = ((rd.begin)[0] << 0) | ((rd.begin)[1] << 8) | ((rd.begin)[2] << 16) | ((rd.begin)[3] << 24);
        rd.begin += sizeof(uint32_t);
        return 1;
    }
    return 0;
}

static void read_frame(struct TrunkStrings strings) {
    if (rd.begin++ >= rd.end) return;
    if (*(rd.begin++) != TRUNK_START_OF_FRAME) return;

    uint32_t ptr_le;
    if (!read_u32(&ptr_le)) return;

    const char *str = try_find_string(strings, ptr_le);
    if (str == NULL) return;

    for (; *str != '\0'; ++str) {
        if (*str == '%' && *(++str) == 'd') {
            uint32_t val;
            if (!read_u32(&val)) {
                fflush(NULL);
                if (isatty(STDOUT_FILENO)) printf("\x1B[31m");
                printf("!!! Not enough data for remaining format parameters. Remaining = `");
                print_escaped(stdout, str - 1);
                printf("` !!!\n");
                if (isatty(STDOUT_FILENO)) printf("\x1B[0m");
                return;
            }

            printf("%d", val);
            continue;
        }
        putchar(*str);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    assert(fd >= 0);

    struct stat st;
    assert(fstat(fd, &st) >= 0);

    void *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(data != MAP_FAILED);

    uint32_t addr, size;
    const uint8_t *trunk_section = find_trunk_section(data, &addr, &size);
    assert(trunk_section != NULL);

    struct TrunkStrings strings = extract_strings(trunk_section, addr, size);

    munmap(data, st.st_size);
    close(fd);

    fprintf(stderr, "Loaded %lu strings.\n", strings.count);
    fprintf(stderr, "\x1B[100;1m--- LOGS START ---\x1B[0m\n");

    ssize_t len;

    for (;;)
    {
        len = read(STDIN_FILENO, rd.buf, sizeof(rd.buf));
        if (len <= 0) break;

        rd.begin = rd.buf;
        rd.end = rd.buf + len;
        while ((rd.begin = memchr(rd.begin, TRUNK_ESCAPE_SYMBOL, rd.end - rd.begin)))
            read_frame(strings);
    }
    return 0;
}

