#include <stdio.h>
#include <stdlib.h>     /* atoi */
#include <string.h>
#include <linux/limits.h>
#include <sys/ptrace.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

#define MAX_HEX_SIZE 1000
#define TMP_BUFFER_SIZE 1000

#ifdef DEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf if(0) printf
#endif

int arch_sizeof_void;

void * parse_hex(char* data, int* l) {
	*l = 0;
	if(data == NULL) return NULL;
	
	unsigned char* pos = data;
	unsigned char* re = malloc(MAX_HEX_SIZE);
	re += 8; // we need to adjust a prefix for this buffer if it does not begin at a word boundary
	while(*pos) {
        sscanf(pos, "%2hhx", &re[*l]);
        pos += 2;
		*l = *l + 1;
	}
	
	return re;
}

// https://stackoverflow.com/questions/7775991/how-to-get-hexdump-of-a-structure-data
void hexDump (
    const char * desc,
    const void * addr,
    const int len
) {
    // Silently ignore silly per-line values.

    int perLine = 16;

    int i;
    unsigned char buff[perLine+1];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL) DEBUG_printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        DEBUG_printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        DEBUG_printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of perLine means new or first line (with line offset).

        if ((i % perLine) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.

            if (i != 0) DEBUG_printf ("  %s\n", buff);

            // Output the offset of current line.

            DEBUG_printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.

        DEBUG_printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % perLine] = '.';
        else
            buff[i % perLine] = pc[i];
        buff[(i % perLine) + 1] = '\0';
    }

    // Pad out last line if not exactly perLine characters.

    while ((i % perLine) != 0) {
        DEBUG_printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    DEBUG_printf ("  %s\n", buff);
}

unsigned char* find_buffer_in_memory(unsigned char* what, int length, unsigned char* begin, unsigned char* end) {
	unsigned char * pos = begin;
	
	while(pos <= end - length) {
		int matchingBytes = 0;
		for(int i = 0; i < length; i++) {
			if(pos[i] == what[i]) {
				matchingBytes++;
			} else {
				break;
			}
		}
		if(matchingBytes == length) {
			// wooho, found the right position!
			return pos;
		}
		else
		{
			pos++;
		}
	}
	
	// not found
	return NULL;
}


int apply_patch(pid_t target, void *target_addr, void *patch, size_t patchlen)
{
	if(-1 == ptrace(PTRACE_ATTACH, target, 0, 0))
		return -1;
	
	DEBUG_printf("PTRACE_ATTACH succeeded\n");
	int re = 0;
	void **target_addr_p = (void**)target_addr;
	void **patch_p = (void**)patch;
	while(patchlen >= sizeof(void*))
	{
		ptrace(PTRACE_POKETEXT, target, target_addr_p++, *(patch_p++));
		DEBUG_printf("PTRACE_POKETEXT succeeded\n");
		patchlen -= sizeof(void*);
	}
	if(patchlen) {
		DEBUG_printf("last round with PTRACE_POKETEXT, need PTRACE_PEEKTEXT\n");
		void *last = (void*)ptrace(PTRACE_PEEKTEXT, target, target_addr_p, 0);
		memcpy((char*)&last, patch_p, patchlen);
		ptrace(PTRACE_POKETEXT, target, target_addr_p, last);
		DEBUG_printf("last PTRACE_POKETEXT succeeded\n");
	}

end:
	if(-1 == ptrace(PTRACE_DETACH, target, 0, 0))
		return -1;
	DEBUG_printf("PTRACE_DETACH succeeded\n");
		
	return re;
}

void* read_range_from_file(char* path, long int from, long int to, long int *len) {
	*len = to - from;
	void* re = malloc(*len);
	if(re == NULL) {
		perror("malloc");
		return NULL;
	}
	unsigned char* rec = (unsigned char*) re;
	
	FILE* fd = fopen(path, "rb");
	if(fd == NULL) {
		perror("fopen");
		free(re);
		return NULL;
	}
	if(0 != fseek(fd, from, SEEK_SET)) {
		perror("fseek");
		free(re);
		return NULL;
	}
	long int remaining = *len;
	while(remaining > 0) {
		int to_read = MIN(16364, remaining);
		int read = fread((void*)rec, to_read, 1, fd);
		int read_bytes = read * to_read;
		DEBUG_printf("just read (bytes): %d\n", read_bytes);
		if(read == 0) {
			// [vvar] might not be readable
			free(re);
			re = NULL;
			break;
		}
		rec += read_bytes;
		remaining -= read_bytes;
	}
	fclose(fd);
	return re;
}

int traverse_mem_ranges(int pid, void* patch_from, void* patch_to, int patch_len) {
	static char * delimiters = " -";
	char maps_file[PATH_MAX];
	char tmp[TMP_BUFFER_SIZE];
	snprintf(maps_file, PATH_MAX, "/proc/%d/maps", pid);
	
	char mem_file[PATH_MAX];
	snprintf(mem_file, PATH_MAX, "/proc/%d/mem", pid); // note: we do this since PTRACE_PEEK is slow
	FILE* fd = fopen(maps_file, "r");
	if(fd == NULL) {
		perror(maps_file);
		return 1;
	}
	while(1) {
	   if (NULL == fgets(tmp, TMP_BUFFER_SIZE, fd)) {
		   // end of file
		   break;
	   }
	   // lets parse a line like this:
	   // 559828520000-559828522000 r--p 00000000 08:20 43471                      /usr/bin/cat
	   DEBUG_printf("we just read: %s\n", tmp);
 	   char * pch = strtok (tmp, delimiters);
	   long int range_begin = strtol(pch, NULL, 16);
	   pch = strtok (NULL, delimiters);
	   long int range_end = strtol(pch, NULL, 16);

	   // unsigned char* range_begin = (unsigned char*) range_begin;
	   // unsigned char* range_end = (unsigned char*) range_end;
	   // DEBUG_printf("parsed as: %p - %p\n", range_begin, range_end);
	   
	   long int process_len;
	   unsigned char* process_memory = (unsigned char*) read_range_from_file(mem_file, range_begin, range_end, &process_len);
	   if(process_memory == NULL) 
			continue;
	
	   unsigned char* match = find_buffer_in_memory(patch_from, patch_len, process_memory, process_memory + process_len);
	   
	   
	   if(match == NULL) continue;
	   
	   long int offset = match - process_memory;
	   long int real_position = range_begin + offset;
	   unsigned char* real_position_p = (unsigned char*)real_position;

	   fprintf(stderr, "Found a match at %p\n", real_position_p);

       unsigned char* patch_to_uc = (unsigned char*)patch_to;
	   while(1) {
			int remainder = real_position % arch_sizeof_void;
			if(remainder == 0) break; // good, word boundary.
			
			match--;
			patch_to_uc--;
			*patch_to_uc = *match;
			
			offset--;
			real_position--;
			patch_len++;
			real_position_p = (unsigned char*)real_position;
			patch_to = (void*)patch_to_uc;
	   }
	   
	   fprintf(stderr, "After adjusting to a word boundary, it is %p\n", real_position_p);
	   
	   hexDump("process memory before", match, patch_len);
	   hexDump("process memory after", patch_to, patch_len);

	   free(process_memory); // not needed any longer
	   	   
	   if(0 == apply_patch(pid, real_position_p, patch_to, patch_len)) {
			fclose(fd);
			return 0;
	   } else {
		   perror("apply_patch");
		   break;
	   }
	}
	
	fclose(fd);
	
	// didn't find it
	fprintf(stderr, "The specified bytes could not be found\n");
	return 2;
}

int main(int argc, char *argv[]) {
	if(argc != 4) {
		fprintf(stderr, "Usage: %s pid hex-encoded-from hex-encoded-to\n", argv[0]);
		return 1;
	}
	
	arch_sizeof_void = sizeof(void*);
	DEBUG_printf("sizeof(void*) is: %d\n", arch_sizeof_void);
	
	int pid = atoi(argv[1]);
	int from_length;
	void* from = parse_hex(argv[2], &from_length);
	if (from == NULL) {
		fprintf(stderr, "Error: unable to hex decode from: %s\n", argv[2]);
		return 2;
	}
	int to_length;
	void* to = parse_hex(argv[3], &to_length);
	if (to == NULL) {
		fprintf(stderr, "Error: unable to hex decode to: %s\n", argv[3]);
		return 2;
	}

	if(from_length != to_length) {
		fprintf(stderr, "Error: from_length (%d) != to_length (%d)\n", from_length, to_length);
		return 2;
	}

	hexDump("from", from, from_length);
	hexDump("to", to, to_length);
	
	int result = traverse_mem_ranges(pid, from, to, from_length);
	fprintf(stderr, (result == 0) ? "Success\n" : "Failure\n");
}
