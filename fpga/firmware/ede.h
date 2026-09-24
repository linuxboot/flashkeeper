// EDE - the Embedded Debug Environment

#ifndef _EDE_H_
#define _EDE_H_
//#include <stdio.h> // debug only

// The size of the parameter stack, in 32-bit cells.
#define EDE_STACK_SIZE 72

// The size of the shell's buffer - this is how many entries can be entered on one line
#define EDE_SHELL_BUFFER_LEN 96

// How many external functions can be registered as EDE words. EDE's built-in words don't count towards this limit.
#define EDE_WORD_TABLE_LEN 32

// Internal Types - adjust if necessary for your compiler
typedef signed int edei32; // ensure is 32 bit signed
typedef signed short edei16; // ensure is 16 bit signed
typedef signed char edei8; // ensure is 8 bit signed

typedef unsigned int edeu32; // ensure is 32 bit unsigned
typedef unsigned short edeu16; // ensure is 16 bit unsigned
typedef unsigned char edeu8; // ensure is 8 bit unsigned

// Start the EDE shell. Provide pointers to:
// get_fn: a function which blocks until a single character is entered, then returns it
// put_fn: a function which prints a single character, then returns it
int ede_shell(char (*get_fn)(), void (*put_fn)(char));

// Register an external function which will operate directly on the stack as an EDE word
// Functions registered this way are void typed and receive only a pointer to the stack and the stack pointer as parameters
int ede_registerfn(const char* name, void (*fn)(edei32*, edeu16*));

typedef struct {
    edei32 r0;
    edei32 r1;
    edei32 r2;
    edei32 r3;
} edefn;

typedef struct {
    edeu32 hash;
    void (*fn)();
    edeu8 vals_in;
    edeu8 vals_out;
} ede_word;

#endif

#ifdef _EDE_IMPLEMENTATION_

#define EDE_FLAG_STACKPARAMS 0xff

ede_word ede_global_word_table[EDE_WORD_TABLE_LEN] = {0};
edeu16 ede_global_word_count = 0;

void ede_puthex(void (*put_fn)(char), edeu32 val, edeu8 bits){
    edeu32 temp;

    for(edei8 i = (edei8)(bits - 4); i >= 0; i -= 4){
        temp = (val >> i) & 0xF;

        if(temp < 10)
            (*put_fn)((char)('0' + temp));
        else
            (*put_fn)((char)('a' + (temp - 10)));
    }
}

int ede_registerfn(const char* name, void (*fn)(edei32*, edeu16*)){
    ede_word w = {0, fn, EDE_FLAG_STACKPARAMS, EDE_FLAG_STACKPARAMS};

    if(ede_global_word_count >= EDE_WORD_TABLE_LEN) return -1; // out of space in the word table

    edeu8 i = 0;
    while(1){
        if(name[i] == '\0') break;

        w.hash <<= 1;
        w.hash ^= name[i];
        i++;
    }
    w.hash ^= ((edeu32) i) << 24;

    ede_global_word_table[ede_global_word_count] = w;
    ede_global_word_count++;

    return 0;
}

// Parses a single hexadecimal digit as a character to an integer value, or returns -1 if the character is not valid hex
edei8 ede_parsehex(edeu8 c){
    if(c >= '0' && c <= '9')
        return (edei8)(c - '0');
    else if(c >= 'A' && c <= 'F')
        return (edei8)((c - 'A') + 10);
    else if(c >= 'a' && c <= 'f')
        return (edei8)((c - 'a') + 10);
    else
        return -1;
}

void ede_shellprompt(void (*put_fn)(char), edeu8 stack_size){
    ede_puthex(put_fn, stack_size, 8);

    (*put_fn)('>');
    (*put_fn)(' ');
}

// Call this directly only for passwords and other secret entry, where you want to display a placeholder character (e.g. *) rather than the typed input
// Fetch a string of up to maxlen characters from the console into the buffer, using get_fn and put_fn. displaychar 0 shows typed characters.
edeu16 ede_getstring_placeholder(char (*get_fn)(void), void (*put_fn)(char), edeu8 *buffer, edeu16 maxlen, edeu8 displaychar){
    edeu16 pos = 0;

    while(1){
        edeu8 c = (*get_fn)(); // fetch a character from the host-provided get_fn

        if(c == 0x7f){ // Handle backspace
            if(pos == 0) continue; // do not allow backspace past beginning of line

            (*put_fn)('\b');
            (*put_fn)(' ');
            (*put_fn)('\b');

            pos--;
            continue;
        }
        
        if(c == '\r' || c == '\n'){ // on newline, print CR+LF and break
            (*put_fn)('\r');
            (*put_fn)('\n');
            break; 
        }

        if(c > 0x7e || c < 0x20) continue; // ignore other control characters - this includes CR
        if(pos >= maxlen) continue; // do not allow keypresses other than backspace or enter if buffer is full

        if(displaychar == 0){
            (*put_fn)(c); // display the typed character with the host-provided put_fn
        }else{
            (*put_fn)(displaychar); // display the placeholder character with the host-provided put_fn (for passwords)
        }

        buffer[pos] = c;
        
        pos++;
    }
    buffer[pos] = '\0';

    return pos;
}

// Fetch a string of up to maxlen characters from the console into the buffer, using get_fn and put_fn, displaying as typed.
edeu16 ede_getstring(char (*get_fn)(void), void (*put_fn)(char), edeu8 *buffer, edeu16 maxlen){
    return ede_getstring_placeholder(get_fn, put_fn, buffer, maxlen, 0);
}

// Execute the word with the given hash against the stack, in place.
// Returns:
//   0  = word not found
//   1  = built-in word executed successfully
//   2  = registered external word executed successfully
//  -1  = the word requested to exit the shell
//  -2  = stack error (underflow, overflow, or division by zero); the stack
//        is left unmodified
int ede_execword(edeu32 hash, void (*put_fn)(char), edei32 *s, edeu16 *sp){
    edei32 t1, t2; // temporaries used in several word definitions

    //printf("Word Hash: 0x%08x\n", hash);

    // Search the built-in word table for the word
    for(edeu16 i=0; i < ede_global_word_count; i++){
        if(hash == ede_global_word_table[i].hash){
            (ede_global_word_table[i].fn)(s, sp);
            return 2;
        }
    }

    // Forth-like built-in words for arithmetic and stack operations
    switch(hash){
        case 0x0100002b: // + - add
            if(*sp < 2) return -2;
            s[*sp - 2] = s[*sp - 2] + s[*sp - 1];
            *sp -= 1;
            break;
        case 0x0100002d: // - - subtract
            if(*sp < 2) return -2;
            s[*sp - 2] = s[*sp - 2] - s[*sp - 1];
            *sp -= 1;
            break;
        case 0x0100002a: // * - multiply
            if(*sp < 2) return -2;
            s[*sp - 2] = s[*sp - 2] * s[*sp - 1];
            *sp -= 1;
            break;
        case 0x0100002f: // / - divide
            if(*sp < 2 || s[*sp - 1] == 0) return -2; // division by zero would hang the core (software divide)
            s[*sp - 2] = s[*sp - 2] / s[*sp - 1];
            *sp -= 1;
            break;
        case 0x04000076: // /mod - divide with remainder
            if(*sp < 2 || s[*sp - 1] == 0) return -2;
            t1 = s[*sp - 2] % s[*sp - 1];
            s[*sp - 1] = s[*sp - 2] / s[*sp - 1];
            s[*sp - 2] = t1;
            break;
        case 0x0300010e: // mod - modulo
            if(*sp < 2 || s[*sp - 1] == 0) return -2;
            s[*sp - 2] = s[*sp - 2] % s[*sp - 1];
            *sp -= 1;
            break;
        case 0x0100002e: // . - print a value
            if(*sp < 1) return -2;
            ede_puthex(put_fn, (edeu32) s[*sp - 1], 32);
            (*put_fn)(' ');
            *sp -= 1;
            break;
        case 0x0200002f: // .s - print the stack (safe with any stack size)
            for(edeu16 i = 0; i < *sp; i++){
                ede_puthex(put_fn, (edeu32) s[i], 32);
                (*put_fn)(' ');
            }
            break;
        case 0x0400023a: // emit - print a character
            if(*sp < 1) return -2;
            (*put_fn)((char) s[*sp - 1]);
            *sp -= 1;
            break;
        case 0x04000246: // drop - drop a value from the stack
            if(*sp < 1) return -2;
            *sp -= 1;
            break;
        case 0x040002f6: // swap - swap the top two values on the stack
            if(*sp < 2) return -2;
            t2 = s[*sp - 1];
            s[*sp - 1] = s[*sp - 2];
            s[*sp - 2] = t2;
            break;
        case 0x0300010a: // dup - duplicate the top value on the stack
            if(*sp < 1 || *sp >= EDE_STACK_SIZE) return -2;
            *sp += 1;
            s[*sp - 1] = s[*sp - 2];
            break;
        case 0x04000218: // over - copy the next value on the stack to the top
            if(*sp < 2 || *sp >= EDE_STACK_SIZE) return -2;
            *sp += 1;
            s[*sp - 1] = s[*sp - 3];
            break;
        case 0x03000162: // rot - rotate the top three values on the stack
            if(*sp < 3) return -2;
            t1 = s[*sp - 1];
            s[*sp - 1] = s[*sp - 3];
            s[*sp - 3] = s[*sp - 2];
            s[*sp - 2] = t1;
            break;
        case 0x0300011f: // bye - exit the EDE shell
        case 0x0400026e: // exit - alias of above
            return -1;
            break;
        default:
            return 0;
    }
    
    return 1;
}

int ede_shell(char (*get_fn)(void), void (*put_fn)(char)){
    edei32 stack[EDE_STACK_SIZE] = {0};
    edeu16 sp = 0; // points to the first UNUSED cell on the stack - that is, if sp is 0, the stack is empty

    // loop per line entered
    while(1){
        edeu8 buffer[EDE_SHELL_BUFFER_LEN + 1] = {0};
        edeu16 buflen;
        edeu16 i = 0;
        edeu8 ok = 1;

        ede_shellprompt(put_fn, (edeu8) sp); // print the prompt, which includes the size of the stack
        buflen = ede_getstring(get_fn, put_fn, buffer, EDE_SHELL_BUFFER_LEN);

        // now process the line in the buffer
        while(i < buflen){
            edeu32 hash = 0;
            edeu32 val = 0;
            edeu16 len = 0;
            edei8 val_valid = 0; // <0 = definitely not value, 0 = possibly value, >0 = definitely value

            // loop over characters of the entry, updating the possible word and value as you go
            while(1){
                edeu8 c = buffer[i];
                edeu32 digit;

                i++;
                if(c == ' ' || c == '\0') break;
                len++;

                if(len == 2 && c == 'x' && val_valid == 2){
                    val_valid = 1; // entry begins with 0x, this is definitely a value unless a non-hex character gets entered
                    continue;
                }else if(len == 1 && c == '0'){
                    val_valid = 2; // temporary flag to detect 0x
                    continue;
                }

                // The user may be entering a value. Update the possible value accordingly.
                val <<= 4;
                digit = (edeu32) ede_parsehex(c);
                val |= digit;
                if(digit > 15) val_valid = -1; // ede_parsehex returned -1 (a non-hex character was entered) and this isn't a valid value anymore

                // The user may also be entering a word. Update the possible hash accordingly.
                hash <<= 1;
                hash ^= c;
            }
            hash ^= ((edeu32) len) << 24; // XOR the length of the word into the high bits of its hash, as a last-ditch try to differentiate long words

            if(len == 0) continue; // don't treat multiple successive spaces as a series of zeroes

            int exec_result;
            // add the entry to the buffer and mark it as a word or a value in the index
            if(val_valid <= 0){ // Possibly not a value, check if it's a word
                exec_result = ede_execword(hash, put_fn, stack, &sp);
                if(exec_result == 0){ // the word was not found
                    if(val_valid < 0){ // This word does not exist and could not be parsed as a value, print ?? and return to a prompt
                        ede_puthex(put_fn, hash, 32);
                        (*put_fn)(' ');
                        (*put_fn)('?');
                        ok = 0;
                        break; 
                    }else{
                        if(sp < EDE_STACK_SIZE){
                            stack[sp] = (edei32) val;
                            sp++;
                        }else{ // stack full: report the error and abandon the line
                            (*put_fn)('!');
                            ok = 0;
                            break;
                        }
                    }
                }else if(exec_result == -1){ // the word wants to exit the shell
                    return 0;
                }else if(exec_result == -2){ // stack error: underflow, overflow, or division by zero
                    (*put_fn)('!');
                    ok = 0;
                    break;
                }else{ // the word executed successfully
                    continue;
                };
            }else{ // definitely a value, just push it to the stack
                if(sp < EDE_STACK_SIZE){
                    stack[sp] = (edei32) val;
                    sp++;
                }else{ // stack full: report the error and abandon the line
                    (*put_fn)('!');
                    ok = 0;
                    break;
                }
            }
        }

        if(ok){
            (*put_fn)('o');
            (*put_fn)('k');
        }
        
        (*put_fn)('\r');
        (*put_fn)('\n');
    }
}

#endif