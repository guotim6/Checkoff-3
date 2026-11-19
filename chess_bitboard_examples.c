/*
 * Chess bitboard learning demos
 *
 * A standalone, interactive console program demonstrating:
 *   1. Moving a piece
 *   2. Capturing a piece
 *   3. Detecting check
 *   4. A simple checkmate position
 *   5. En passant
 *
 * Board indexing: a1 = 0, b1 = 1, ..., h8 = 63.
 * Compile (GCC/Clang): gcc -std=c11 -Wall -Wextra -O2 chess_bitboard_examples.c -o chess_demo
 * Run: ./chess_demo       (Windows: chess_demo.exe)
 *
 * This is a teaching demo, not a complete chess engine. In particular,
 * the checkmate routine below is deliberately limited to the supplied
 * king-only defense example; a full engine must test every legal response.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

enum Piece { WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK, PIECE_N }; 
static const char *piece_name[PIECE_N] = {
    "White pawn", "White knight", "White bishop", "White rook", "White queen", "White king",
    "Black pawn", "Black knight", "Black bishop", "Black rook", "Black queen", "Black king"
};
static const char piece_char[PIECE_N] = {'P','N','B','R','Q','K','p','n','b','r','q','k'};

typedef struct { uint64_t bb[PIECE_N]; } Position;

static uint64_t bit(int sq) { return UINT64_C(1) << sq; }
static int file_of(int sq) { return sq & 7; }
static int rank_of(int sq) { return sq >> 3; }
static bool on_board(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }

static uint64_t occupancy(const Position *p, bool white) {
    uint64_t result = 0;
    int first = white ? WP : BP, last = white ? WK : BK;
    for (int i = first; i <= last; ++i) result |= p->bb[i];
    return result;
}
static uint64_t all_occupancy(const Position *p) {
    uint64_t result = 0;
    for (int i = 0; i < PIECE_N; ++i) result |= p->bb[i];
    return result;
}
static int piece_at(const Position *p, int sq) {
    uint64_t mask = bit(sq);
    for (int i = 0; i < PIECE_N; ++i) if (p->bb[i] & mask) return i;
    return -1;
}
static void put(Position *p, enum Piece piece, int sq) { p->bb[piece] |= bit(sq); }
static void clear_square(Position *p, int sq) {
    uint64_t mask = ~bit(sq);
    for (int i = 0; i < PIECE_N; ++i) p->bb[i] &= mask;
}
static void print_board(const Position *p) {
    puts("\n    a b c d e f g h");
    puts("   -----------------");
    for (int r = 7; r >= 0; --r) {
        printf("%d | ", r + 1);
        for (int f = 0; f < 8; ++f) {
            int pc = piece_at(p, r * 8 + f);
            printf("%c ", pc < 0 ? '.' : piece_char[pc]);
        }
        printf("| %d\n", r + 1);
    }
    puts("   -----------------");
    puts("    a b c d e f g h\n");
}
static void print_bitboards(const Position *p) {
    for (int i = 0; i < PIECE_N; ++i)
        if (p->bb[i]) printf("  %-12s 0x%016" PRIx64 "\n", piece_name[i], p->bb[i]);
}
static int square_from_name(const char *s) {
    if (!s || strlen(s) != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return -1;
    return (s[1] - '1') * 8 + (s[0] - 'a');
}
static void describe_move(int from, int to) {
    printf("Move: %c%d -> %c%d\n", 'a' + file_of(from), 1 + rank_of(from),
           'a' + file_of(to), 1 + rank_of(to));
}

/* Clear the source bit and set the destination bit. */
static bool move_piece(uint64_t *pieces, int from, int to) {
    if (from < 0 || from >= 64 || to < 0 || to >= 64 || !(*pieces & bit(from))) return false;
    *pieces &= ~bit(from);
    *pieces |= bit(to);
    return true;
}

/* Capture on the destination, then move the attacking piece there. */
static bool capture_piece(Position *p, enum Piece attacker, enum Piece victim, int from, int to) {
    if (!(p->bb[attacker] & bit(from)) || !(p->bb[victim] & bit(to))) return false;
    p->bb[victim] &= ~bit(to);
    p->bb[attacker] &= ~bit(from);
    p->bb[attacker] |= bit(to);
    return true;
}

/* Is target attacked by the selected side? Handles all piece attack shapes and blockers. */
static bool square_attacked(const Position *p, int target, bool by_white) {
    int tf = file_of(target), tr = rank_of(target);
    int first = by_white ? WP : BP, last = by_white ? WK : BK;
    for (int pc = first; pc <= last; ++pc) {
        uint64_t pieces = p->bb[pc];
        while (pieces) {
            int from = __builtin_ctzll(pieces);
            pieces &= pieces - 1;
            int df = tf - file_of(from), dr = tr - rank_of(from);
            int adf = abs(df), adr = abs(dr);
            bool attacks = false;
            switch (pc) {
                case WP: attacks = (adr == 1 && adf == 1 && dr == 1); break;
                case BP: attacks = (adr == 1 && adf == 1 && dr == -1); break;
                case WN: case BN: attacks = (adf == 1 && adr == 2) || (adf == 2 && adr == 1); break;
                case WK: case BK: attacks = adf <= 1 && adr <= 1 && (adf + adr != 0); break;
                default: {
                    bool diagonal = adf == adr && adf != 0;
                    bool straight = (df == 0) != (dr == 0);
                    bool slider = (pc == WB || pc == BB) ? diagonal :
                                  (pc == WR || pc == BR) ? straight :
                                  (pc == WQ || pc == BQ) ? (diagonal || straight) : false;
                    if (slider) {
                        int sf = (df > 0) - (df < 0), sr = (dr > 0) - (dr < 0);
                        int f = file_of(from) + sf, r = rank_of(from) + sr;
                        attacks = true;
                        while (f != tf || r != tr) {
                            if (all_occupancy(p) & bit(r * 8 + f)) { attacks = false; break; }
                            f += sf; r += sr;
                        }
                    }
                    break;
                }
            }
            if (attacks) return true;
        }
    }
    return false;
}
static int king_square(const Position *p, bool white) {
    uint64_t k = p->bb[white ? WK : BK];
    return k ? __builtin_ctzll(k) : -1;
}
static bool in_check(const Position *p, bool white) {
    int k = king_square(p, white);
    return k >= 0 && square_attacked(p, k, !white);
}

/* Demonstration-specific mate test: the supplied position has only the black king
   as a defender. It tests every king destination, including capturing the queen. */
static bool demo_checkmate_king_only(const Position *p) {
    int k = king_square(p, false);
    if (k < 0 || !in_check(p, false)) return false;
    for (int df = -1; df <= 1; ++df) for (int dr = -1; dr <= 1; ++dr) {
        if (df == 0 && dr == 0) continue;
        int f = file_of(k) + df, r = rank_of(k) + dr;
        if (!on_board(f, r)) continue;
        int to = r * 8 + f, occupant = piece_at(p, to);
        if (occupant >= BP && occupant <= BK) continue; /* own piece */
        Position next = *p;
        clear_square(&next, to); /* remove any captured white piece */
        next.bb[BK] &= ~bit(k);
        next.bb[BK] |= bit(to);
        if (!in_check(&next, false)) return false;
    }
    return true;
}

/* White en passant: the captured pawn is on to-8, not on the landing square. */
static bool en_passant_white(Position *p, int from, int to) {
    int captured = to - 8;
    if (from < 0 || from >= 64 || to < 0 || to >= 64 || captured < 0 || captured >= 64) return false;
    if (!(p->bb[WP] & bit(from)) || !(p->bb[BP] & bit(captured)) || (all_occupancy(p) & bit(to))) return false;
    if (rank_of(from) != 4 || rank_of(to) != 5 || abs(file_of(from) - file_of(to)) != 1) return false;
    p->bb[WP] &= ~bit(from);
    p->bb[BP] &= ~bit(captured);
    p->bb[WP] |= bit(to);
    return true;
}

static void wait_for_enter(void) {
    char line[16];
    printf("\nPress Enter to return to the menu...");
    (void)fgets(line, sizeof line, stdin);
}
static void demo_move(void) {
    Position p = {0}; put(&p, WP, 12); put(&p, WK, 4); put(&p, BK, 60);
    puts("=== 1. MOVING A PIECE ===\nWhite pawn moves from e2 to e4.");
    puts("Before:"); print_board(&p); print_bitboards(&p);
    (void)move_piece(&p.bb[WP], 12, 28);
    puts("After:"); print_board(&p); print_bitboards(&p);
    puts("The pawn's source bit was cleared with AND NOT; its destination bit was set with OR.");
}
static void demo_capture(void) {
    Position p = {0}; put(&p, WR, 0); put(&p, BP, 48); put(&p, WK, 4); put(&p, BK, 60);
    puts("=== 2. CAPTURING A PIECE ===\nWhite rook captures the black pawn on a7.");
    puts("Before:"); print_board(&p); print_bitboards(&p);
    (void)capture_piece(&p, WR, BP, 0, 48);
    puts("After:"); print_board(&p); print_bitboards(&p);
    puts("The black pawn's bit is removed, then the rook's bit moves from a1 to a7.");
}
static void demo_check(void) {
    Position p = {0}; put(&p, WR, 52); put(&p, BK, 60); put(&p, WK, 0);
    puts("=== 3. DETECTING CHECK ===\nWhite rook on e7 attacks the black king on e8.");
    print_board(&p);
    printf("Black king is in check: %s\n", in_check(&p, false) ? "YES" : "NO");
    puts("The test intersects the enemy attack map with a one-bit mask for the king's square.");
}
static void demo_checkmate(void) {
    Position p = {0}; put(&p, WK, 45); put(&p, BK, 63); put(&p, WQ, 54);
    puts("=== 4. CHECKMATE (KING-ONLY DEFENSE EXAMPLE) ===\nWhite queen on g7 is protected by the white king on f6.");
    print_board(&p);
    printf("Black king is in check: %s\n", in_check(&p, false) ? "YES" : "NO");
    printf("Checkmate in this demonstration: %s\n", demo_checkmate_king_only(&p) ? "YES" : "NO");
    puts("A complete engine must also test captures by other pieces and blocks by other pieces.");
}
static void demo_en_passant(void) {
    Position p = {0}; put(&p, WP, 36); put(&p, BP, 35); put(&p, WK, 4); put(&p, BK, 60);
    puts("=== 5. EN PASSANT ===\nBlack has just played d7-d5. White pawn on e5 captures on d6.");
    puts("Before:"); print_board(&p); print_bitboards(&p);
    (void)en_passant_white(&p, 36, 43);
    puts("After:"); print_board(&p); print_bitboards(&p);
    puts("White lands on d6; the black pawn is removed from d5 (behind the destination).");
    puts("A real game must also track that the two-square pawn move happened immediately before this move.");
}
static void show_menu(void) {
    puts("\n========================================");
    puts("       CHESS BITBOARD CODE EXAMPLES");
    puts("========================================");
    puts("1. Moving a piece");
    puts("2. Capturing a piece");
    puts("3. Detecting check");
    puts("4. Checkmate example");
    puts("5. En passant");
    puts("6. Exit");
    printf("Choose an example (1-6): ");
}
int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    char input[32];
    for (;;) {
        show_menu();
        if (!fgets(input, sizeof input, stdin)) break;
        switch (input[0]) {
            case '1': demo_move(); wait_for_enter(); break;
            case '2': demo_capture(); wait_for_enter(); break;
            case '3': demo_check(); wait_for_enter(); break;
            case '4': demo_checkmate(); wait_for_enter(); break;
            case '5': demo_en_passant(); wait_for_enter(); break;
            case '6': puts("Goodbye!"); return 0;
            default: puts("Please enter a number from 1 to 6."); break;
        }
    }
    return 0;
}
