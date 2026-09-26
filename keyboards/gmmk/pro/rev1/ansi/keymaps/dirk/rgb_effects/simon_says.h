// Simon Says: the keyboard plays a growing sequence of letter keys and you play
// it back.
//
// A round is two halves. First the board replays the whole sequence it has, one
// key per slot, in its own colour. Then it goes quiet and waits, and every key
// you press has to be the next key of that sequence. Get all of them right and
// the board appends one random letter and replays the lot again, so a round that
// goes well looks exactly like "the same keys once more, and then one I have not
// seen". Press anything else and the sequence collapses back to a single random
// letter.
//
// Either way there is a pause before the next sequence is played - longer after a
// mistake than after a good round, but the same idea: the board is not asking you
// anything for a moment, and its answer goes on sweeping the column while it
// waits. See SIMON_SAYS_NEW_GAME_COOLDOWN_MS and SIMON_SAYS_ROUND_COOLDOWN_MS.
//
// The key you pressed is lit as it is judged, green if it was the next one of the
// sequence and red if it was not, fading out on the same shared curve as the sweep.
// Each key keeps its own verdict on its own clock, so getting a run of them right
// leaves a trail of greens behind it instead of only the newest one. That is the
// only per key feedback the game gives, and through the long middle of a sequence
// it is also the only feedback there is, because the round has not ended and the
// sweep has nothing to say.
//
// A mistake also ends with a number on the board: after SIMON_SAYS_PRINT_DELAY_MS
// the length of the sequence you just lost is printed on the number row, scrolling,
// which is the same no-font trick calculator.h prints its answers with. It is
// inside the new game pause, so it lands on a board that is otherwise dark and
// costs the round nothing.
//
// The four keys of the right hand column are the score. They are not letters, so
// nothing in the game can ever light one, which is what makes them safe to give
// over to the answer: a completed round sweeps them green from the bottom up and a
// mistake sweeps them red from the top down, each key fading out as it comes on.
// The sweep is a side show and never gates the game, so the next sequence can
// already be replaying underneath it.
//
// Presses are read off the matrix through helpers/held_keys.h rather than off
// g_last_hit_tracker, for the reason that helper sets out: the tracker records
// presses only, so a key still under your finger is indistinguishable from one you
// let go of a moment ago, and it cannot hold a sequence anyway.

#include "matrix.h"
#include "timer.h"

#include "helpers/keycodes.h"
#include "helpers/map_colors.h"
#include "helpers/rgb_print.h"
#include "helpers/held_keys.h" // which pulls in reactive_fade.h

// ---------------------------------------------------------------------------
// The letters, and the sequence
// ---------------------------------------------------------------------------

// a-z, as LED indices, in reading order. Random draws index straight into this,
// so the sequence itself stores a position here rather than a full LED index.
#define SIMON_SAYS_LETTERS 26
static const uint8_t simon_says_letters[SIMON_SAYS_LETTERS] = {
    K_Q, K_W, K_E, K_R, K_T, K_Y, K_U, K_I, K_O, K_P, // top row
    K_A, K_S, K_D, K_F, K_G, K_H, K_J, K_K, K_L,      // home row
    K_Z, K_X, K_C, K_V, K_B, K_N, K_M,                // bottom row
};

// How many keys the sequence may grow to, i.e. how long a perfect run has to be
// before the game stops getting harder. Each step is one byte, so 64 is 64 bytes
// of RAM and a run nobody is going to reach; raise it if you want to be sure.
#ifndef SIMON_SAYS_SEQUENCE_MAX
#    define SIMON_SAYS_SEQUENCE_MAX 64
#endif
STATIC_ASSERT(SIMON_SAYS_SEQUENCE_MAX >= 1, "the game needs room for at least one key");
STATIC_ASSERT(SIMON_SAYS_SEQUENCE_MAX <= UINT8_MAX, "a step is a uint8_t index into simon_says_letters");

// The key that shows each digit, indexed by the digit itself rather than by its
// character, so a score is a lookup and not a switch. 0 first because it is the one
// digit that is not next in reading order on the keycaps.
static const uint8_t simon_says_digits[10] = {
    K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
};
STATIC_ASSERT(sizeof(simon_says_digits) / sizeof(simon_says_digits[0]) == 10, "a uint8_t score can be any of the ten digits");

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

// How long one key of the replay holds the display. The key is lit for
// SIMON_SAYS_FLASH_MS of that and dark for the rest, and the dark part is what
// makes one key legible as one key - a sequence that never goes dark reads as a
// single smear of colour.
#ifndef SIMON_SAYS_STEP_MS
#    define SIMON_SAYS_STEP_MS 450
#endif
#ifndef SIMON_SAYS_FLASH_MS
#    define SIMON_SAYS_FLASH_MS 300
#endif
STATIC_ASSERT(SIMON_SAYS_FLASH_MS > 0 && SIMON_SAYS_FLASH_MS < SIMON_SAYS_STEP_MS, "SIMON_SAYS_FLASH_MS must leave a dark gap inside SIMON_SAYS_STEP_MS");

// How far apart the four score keys come on, and how long each then takes to fade
// to black. The fade window is the speed slider's, like every other reactive
// effect in this keymap, so SIMON_SAYS_FADE_MIN_MS is the fastest fade the knob
// asks for and SIMON_SAYS_FADE_MAX_MS the slowest.
//
// The window also has to fit in a uint16_t, since that is what
// reactive_fade_window() hands back.
#ifndef SIMON_SAYS_SWEEP_STEP_MS
#    define SIMON_SAYS_SWEEP_STEP_MS 120
#endif
#ifndef SIMON_SAYS_FADE_MIN_MS
#    define SIMON_SAYS_FADE_MIN_MS 250
#endif
#ifndef SIMON_SAYS_FADE_MAX_MS
#    define SIMON_SAYS_FADE_MAX_MS 1500
#endif
STATIC_ASSERT(SIMON_SAYS_FADE_MIN_MS > 0 && SIMON_SAYS_FADE_MAX_MS > SIMON_SAYS_FADE_MIN_MS, "SIMON_SAYS_FADE_MAX_MS must exceed SIMON_SAYS_FADE_MIN_MS");
STATIC_ASSERT(SIMON_SAYS_FADE_MAX_MS <= UINT16_MAX, "the fade window is a uint16_t");
STATIC_ASSERT(SIMON_SAYS_SWEEP_STEP_MS > 0, "the sweep has to move");

// The pause between one game and the next (after making a mistake)
#ifndef SIMON_SAYS_NEW_GAME_COOLDOWN_MS
#    define SIMON_SAYS_NEW_GAME_COOLDOWN_MS 10000
#endif
// The pause between one round and the next.
#ifndef SIMON_SAYS_ROUND_COOLDOWN_MS
#    define SIMON_SAYS_ROUND_COOLDOWN_MS 2000
#endif

// How long after a mistake the score is printed, counted from the keystroke. It
// sits inside SIMON_SAYS_NEW_GAME_COOLDOWN_MS, which is what the delay is for: the
// red sweep is well over by the time the number appears, so the two read as "you
// lost" and then "here is how far you got" rather than as one long noise. Raise the
// cooldown if you set this above it - the print then lands during the next round
// and takes the board over while you are trying to play it.
//
// 0 prints on the same frame as the mistake.
#ifndef SIMON_SAYS_PRINT_DELAY_MS
#    define SIMON_SAYS_PRINT_DELAY_MS 2500
#endif

// The score is printed on the number row, the keys the digits are actually on, and
// scrolls left to right. Three slots because simon_says_length is a uint8_t, so
// three digits is the most it can ever need. There is no font involved anywhere in
// this - it is the same trick calculator.h uses to print its answers, right down to
// the shared rgb_print() that draws it.
#define SIMON_SAYS_PRINT_DIGITS 3
#define SIMON_SAYS_PRINT_KEY_MS 4096 // how long a digit is lit, in rgb_print() ticks
#define SIMON_SAYS_PRINT_GAP_MS 2048 // and the dark part of its slot after that
STATIC_ASSERT(SIMON_SAYS_PRINT_KEY_MS > 0, "a printed digit has to be lit for something");

// ---------------------------------------------------------------------------
// The score column
// ---------------------------------------------------------------------------

// The four keys the answer is given on, in sweep order: bottom to top, so a
// correct round walks up the column and a mistake walks back down it. See
// helpers/keycodes.h for why these are named K_POS_* rather than after the
// keycodes layer 0 puts on them.
#define SIMON_SAYS_PROGRESS_STEPS 4
static const uint8_t simon_says_progress_keys[SIMON_SAYS_PROGRESS_STEPS] = {
    K_F13,
    K_END,
    K_HOME,
    K_DELETE,
};
STATIC_ASSERT(sizeof(simon_says_progress_keys) / sizeof(simon_says_progress_keys[0]) == SIMON_SAYS_PROGRESS_STEPS, "the array has to hold every step the sweep takes");

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

// Sentinels for the "nothing here" values below, which have to be values no real
// entry can take: a 32 bit age in milliseconds, a 32 bit timer stamp, and a position
// in simon_says_letters.
#define SIMON_SAYS_AGE_OFF UINT32_MAX // this LED has nothing on it
#define SIMON_SAYS_NO_TIME UINT32_MAX // this LED has not been judged yet
#define SIMON_SAYS_NO_LETTER UINT8_MAX

// Which way the score column is sweeping, if at all. 0 doubles as "no sweep", so
// a freshly selected effect cannot inherit one.
enum simon_says_sweep_kind {
    SIMON_SAYS_SWEEP_NONE = 0,
    SIMON_SAYS_SWEEP_GOOD = 1, // green, bottom to top
    SIMON_SAYS_SWEEP_BAD  = 2, // red, top to bottom
};

// The sequence, one position into simon_says_letters per step, and how much of it
// there currently is.
static uint8_t simon_says_sequence[SIMON_SAYS_SEQUENCE_MAX];
static uint8_t simon_says_length = 1;

// How far into the sequence you are, i.e. how many of its keys you have entered
// correctly so far this round. Only meaningful while the board is waiting.
static uint8_t simon_says_typed = 0;

// True while the board is replaying the sequence, false while it is waiting for
// you. One flag rather than an enum of phases because there are only the two, and
// it is read once per LED in the paint.
static bool simon_says_showing = true;

// True between rounds: SIMON_SAYS_ROUND_COOLDOWN_MS after a completed round,
// SIMON_SAYS_NEW_GAME_COOLDOWN_MS after a mistake. The next sequence is already dealt, but the
// board is neither playing it nor listening for it, and the letters are black until
// the pause is up.
//
// A third phase rather than a third value of simon_says_showing, so that "not
// showing" never has to mean both "your turn" and "not yet"; the paint has to tell
// those apart. Named "paused" rather than "waiting" for a second reason: the rest
// of this file already talks about the board waiting for you, which is the other one
// of the two phases it is not showing.
static bool simon_says_paused = false;

// Which of the two pauses is running, copied out of the macro when the pause began
// so that the two can share one countdown. Read only while simon_says_paused is
// true, i.e. only after a judgement has set it.
static uint32_t simon_says_pause = 0;

// When the replay now running began, when the sweep now running began, and when the
// pause now running began. All three are timer_read32() stamps, and all three are
// compared by unsigned subtraction so they stay correct across the 32 bit wrap, the
// way simon_says_step_sweep() and simon_says_step_show() below do it.
static uint32_t simon_says_show_started  = 0;
static uint32_t simon_says_sweep_started = 0;
static uint32_t simon_says_pause_started = 0;
static uint8_t  simon_says_sweep         = SIMON_SAYS_SWEEP_NONE;

// The level each letter was at on the previous frame, kept only so that its press
// can be found as an edge - held_keys_switch_down() answers "is it down now", and
// the game needs to know "did it just go down".
static uint8_t simon_says_shadow[SIMON_SAYS_LETTERS];

// How long ago each LED was lit, or SIMON_SAYS_AGE_OFF. Built once per frame and
// then only read, because the effect runs once per RGB_MATRIX_LED_PROCESS_LIMIT
// chunk (~20 LEDs, so five times a frame at this keyboard's 98) and each chunk
// only renders its own slice: computing this per chunk would show the first chunks
// of a frame rendering the state of the scan they took themselves and the last
// chunks the state of the scan before that.
static uint32_t simon_says_age[RGB_MATRIX_LED_COUNT];

// When each LED was last judged, and whether that judgement was right.
//
// Per LED, not one slot for "the key you pressed last". A player working through a
// long sequence has several keys lit at once - three greens in a row, or two greens
// and then the red that ended the round - and each has to keep its own colour for
// its own fade. A single slot is exactly what stops that happening: the next
// judgement overwrites the last, so the key pressed a moment ago drops out of green
// and back to whatever the rest of the effect makes of it.
//
// Stamped and left to the renderer to age rather than kept in simon_says_age[],
// which is cleared and rebuilt every frame, because a verdict has to outlive the
// frame it was made on - mid sequence it is often the only thing on the board for
// whole seconds at a time.
//
// SIMON_SAYS_NO_TIME until an LED is first judged, which is deliberately not zero:
// a stamp of zero reads as "judged at boot", and a fade window longer than the time
// the effect has been selected for would then light every key on the board. Set by
// the clear in the init branch of simon_says_step().
static uint32_t simon_says_verdict_at[RGB_MATRIX_LED_COUNT];
static bool     simon_says_verdict_good[RGB_MATRIX_LED_COUNT]; // only read where the stamp is live

// The score waiting to be printed, and the score being printed.
//
// Two flags rather than one because there are two things going on: a mistake arms
// the print to happen SIMON_SAYS_PRINT_DELAY_MS later, and then the print itself
// runs for a good few hundred milliseconds after that. Folding them together would
// mean either re-arming forever or losing the delay.
//
// The stamp is a start rather than a deadline, compared by unsigned subtraction like
// every other timestamp in this file, so it survives the 32 bit wrap.
static int      simon_says_print_buffer[SIMON_SAYS_PRINT_DIGITS]; // LED indices, most significant digit first
static uint8_t  simon_says_print_count   = 0;
static bool     simon_says_print_queued  = false;
static bool     simon_says_print_running = false;
static uint32_t simon_says_print_started = 0;

// timer_read32() for the frame being rendered, and the fade window read from the
// speed slider for it. Read once a frame for the same reason as simon_says_age[],
// and because the knob can move under us mid-frame: the renderer and the sweep
// bookkeeping have to agree on the window or a key would fade at one rate and be
// retired at another.
static uint32_t simon_says_now  = 0;
static uint16_t simon_says_fade = SIMON_SAYS_FADE_MAX_MS;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// One step of the sequence, as a random position in simon_says_letters. Every
// letter is equally likely and the draw is made with replacement, so a round can
// repeat the key before it - which is the whole point of the game.
static uint8_t simon_says_random(void) {
    return random8_max(SIMON_SAYS_LETTERS);
}

// Is this one of the four score keys? A four element search per LED per frame,
// which is nothing next to the matrix scan it happens after.
static bool simon_says_is_progress_key(uint8_t led) {
    for (uint8_t p = 0; p < SIMON_SAYS_PROGRESS_STEPS; p++) {
        if (simon_says_progress_keys[p] == led) return true;
    }
    return false;
}

// The two answers, at full saturation and full value so that what fades out is the
// colour and not a dimmer bulb. Shared by the sweep and the verdict flash, so the
// two cannot drift into different greens or different reds.
static HSV simon_says_verdict_hsv(bool good) {
    return good ? (HSV){HSV_GREEN} : (HSV){HSV_RED};
}

static HSV simon_says_sweep_hsv(void) {
    return simon_says_verdict_hsv(simon_says_sweep == SIMON_SAYS_SWEEP_GOOD);
}

// Light this LED with the answer to the keypress that just happened, and start its
// fade.
//
// Touches only the one LED, so the verdicts already on the board are left to finish
// their own fades - which is the whole point of them being per LED. Re-judging a
// key restarts its fade rather than adding to it, so holding a key down and tapping
// it again does not stack up brightness.
//
// Takes an LED rather than a position in simon_says_letters so that it can also
// serve the restart key, which is not a letter.
static void simon_says_verdict(uint8_t led, bool good) {
    simon_says_verdict_at[led]   = simon_says_now;
    simon_says_verdict_good[led] = good;
}

// The same, for a key of the sequence. Called on both sides of a judgement, so the
// two cannot disagree about which of them is right.
static void simon_says_judge_led(uint8_t letter, bool good) {
    simon_says_verdict(simon_says_letters[letter], good);
}

// Arm the score to be printed, this many milliseconds from now.
//
// Most significant digit first, because that is the only order a number reads in.
// A one digit score is one digit: the divisor walk below is what drops the leading
// zeroes a fixed width buffer would otherwise print, and a score of 0 still prints
// its one digit rather than nothing.
static void simon_says_arm_print(uint8_t score) {
    // The largest power of ten that divides into score, so 7 gives 1 and 64 gives
    // 10. score is a uint8_t, so divisor tops out at 100 and the multiply below
    // cannot overflow an int.
    uint8_t divisor = 1;
    while (score / (divisor * 10) != 0) {
        divisor *= 10;
    }

    uint8_t count = 0;
    while (divisor != 0 && count < SIMON_SAYS_PRINT_DIGITS) {
        simon_says_print_buffer[count++] = simon_says_digits[(score / divisor) % 10];
        divisor /= 10;
    }

    simon_says_print_count   = count;
    simon_says_print_started = simon_says_now;
    simon_says_print_queued  = true;
}

// Deal a fresh game: one random letter, no progress, and no round under way - the
// caller then chooses the phase, which is the only difference between selecting the
// effect and fumbling a key. Deliberately leaves any sweep and any verdicts already
// on the board alone, see the call in simon_says_step().
static void simon_says_deal(void) {
    simon_says_length      = 1;
    simon_says_sequence[0] = simon_says_random();
    simon_says_typed       = 0;
    simon_says_showing     = false;
    simon_says_paused      = false;
}

// ---------------------------------------------------------------------------
// The game
// ---------------------------------------------------------------------------

// Judge one keypress against the sequence. A key is either the next one, or it
// is not and the round is over. Either way the round is over with a pause before
// the next one, so both ends of this function end by arming one.
//
// The key is lit green or red on the way through, which is the only place the
// player learns whether an individual keystroke counted - mid sequence the round
// has not ended and the sweep has nothing to say.
static void simon_says_answer(uint8_t letter) {
    if (letter != simon_says_sequence[simon_says_typed]) {
        // Wrong, so the key you pressed goes red, the score column goes red, and
        // the sequence collapses back to a single random letter - a new one rather
        // than the old first key, which is what "reset" has to mean for a game
        // whose next round starts at length one anyway.
        //
        // Dealt now, shown after SIMON_SAYS_NEW_GAME_COOLDOWN_MS. The new letter is on the
        // board before the pause is up but is not part of a round yet, so a key
        // pressed during the pause is not judged against it, and the pause survives
        // however hard the wrong key is mashed.
        //
        // The score is read before the deal, not after: simon_says_deal() resets the
        // length to one, so afterwards there would be nothing left to print but a 1.
        // What is printed is the length of the sequence just lost, i.e. the round you
        // were on - not how far into it you got, which is simon_says_typed, and is
        // the other number you might have wanted here.
        uint8_t lost = simon_says_length - 1;

        simon_says_sweep         = SIMON_SAYS_SWEEP_BAD;
        simon_says_sweep_started = simon_says_now;
        simon_says_deal();
        simon_says_judge_led(letter, false);
        simon_says_arm_print(lost);
        simon_says_pause         = SIMON_SAYS_NEW_GAME_COOLDOWN_MS;
        simon_says_paused        = true;
        simon_says_pause_started = simon_says_now;
        return;
    }

    if (simon_says_typed + 1 < simon_says_length) {
        // Right, but there is more of the sequence still to enter. Nothing else
        // happens: the board is simply quiet again, which is what makes the green
        // on the key the whole of the answer.
        simon_says_judge_led(letter, true);
        simon_says_typed++;
        return;
    }

    // The whole sequence, right. Green on the key, green on the column, and one more
    // letter to play: the replay that follows is the sequence just entered plus
    // this, which is why it starts on the key that was just pressed.
    simon_says_sweep         = SIMON_SAYS_SWEEP_GOOD;
    simon_says_sweep_started = simon_says_now;
    simon_says_judge_led(letter, true);

    // Once the sequence is as long as the buffer allows there is nowhere to put
    // another letter, so the game stays at full length and keeps asking for it
    // again. Better that than wrapping onto itself and asking for a sequence the
    // player never saw.
    if (simon_says_length < SIMON_SAYS_SEQUENCE_MAX) {
        simon_says_sequence[simon_says_length++] = simon_says_random();
    }

    // Then a pause, by the same mechanism a mistake gets, so that a good round reads
    // as a good round rather than as the next question arriving on top of the
    // green. A reward of 0 is the original behaviour: the sweep starts and the next
    // sequence is already under way on this same frame.
    simon_says_typed         = 0;
    simon_says_showing       = false;
    simon_says_pause         = SIMON_SAYS_ROUND_COOLDOWN_MS;
    simon_says_paused        = true;
    simon_says_pause_started = simon_says_now;
}

// ---------------------------------------------------------------------------
// The animations
// ---------------------------------------------------------------------------

// How long a sweep lasts from its first key to the last having gone black, so it
// can be retired once there is nothing left to draw. The fade window is part of
// it, which is why this is asked for rather than computed: the knob decides.
static uint32_t simon_says_sweep_length(void) {
    return (SIMON_SAYS_PROGRESS_STEPS - 1) * (uint32_t)SIMON_SAYS_SWEEP_STEP_MS + simon_says_fade;
}

// Light the score column. Each key comes on at full colour the moment it is
// reached and then fades to black, which is the "blink, then fade" of the effect
// the column is for - the fade is the shared reactive one, so the speed slider
// moves it like it moves everything else.
static void simon_says_step_sweep(void) {
    if (simon_says_sweep == SIMON_SAYS_SWEEP_NONE) return;

    uint32_t elapsed = simon_says_now - simon_says_sweep_started;

    for (uint8_t p = 0; p < SIMON_SAYS_PROGRESS_STEPS; p++) {
        // p is the step, i.e. how far into the sweep we are, and it is what the
        // delay is measured from - so the p-th step always lights p intervals in.
        // Which key that is depends on the direction: the array is ordered bottom
        // to top, so green walks it forwards and red walks it backwards, and
        // either way step 0 is the end the sweep starts at.
        uint8_t  key = (simon_says_sweep == SIMON_SAYS_SWEEP_GOOD) ? p : (SIMON_SAYS_PROGRESS_STEPS - 1 - p);
        uint32_t due = (uint32_t)p * SIMON_SAYS_SWEEP_STEP_MS;

        if (elapsed < due) continue; // not reached yet

        uint32_t age = elapsed - due;
        // Past its fade the key is black, and staying out of simon_says_age[]
        // says exactly that. The same offset the renderer will compute is the
        // test, so the two cannot disagree about where the fade ends.
        if (reactive_fade_offset(age, simon_says_fade) == 255) continue;

        simon_says_age[simon_says_progress_keys[key]] = age;
    }

    if (elapsed >= simon_says_sweep_length()) simon_says_sweep = SIMON_SAYS_SWEEP_NONE;
}

// Run the replay, and hand over to the player at the end of it.
static void simon_says_step_show(void) {
    uint32_t elapsed = simon_says_now - simon_says_show_started;
    uint32_t total   = (uint32_t)simon_says_length * SIMON_SAYS_STEP_MS;

    if (elapsed >= total) {
        // Replay over. The last key has been dark for SIMON_SAYS_STEP_MS -
        // SIMON_SAYS_FLASH_MS by now, so the handover happens in the dark and
        // nothing pops.
        simon_says_showing = false;
        simon_says_typed   = 0;
        return;
    }

    // Which key this slot is showing. elapsed < total bounds the division, so
    // step cannot run off the end of the sequence.
    //
    // step is a uint8_t because it indexes a sequence that is at most
    // SIMON_SAYS_SEQUENCE_MAX long, but since is not: it is the offset inside the
    // slot, and SIMON_SAYS_STEP_MS is allowed to be larger than 255, so a uint8_t
    // here wraps partway through every slot and the key it is showing never goes
    // dark at all.
    uint8_t  step  = elapsed / SIMON_SAYS_STEP_MS;
    uint32_t since = elapsed % SIMON_SAYS_STEP_MS;

    if (since < SIMON_SAYS_FLASH_MS) {
        // Age 0 rather than since: a replayed key is a hard flash, not a fade, and
        // the gap to the next one is what draws the rhythm.
        simon_says_age[simon_says_letters[simon_says_sequence[step]]] = 0;
    }
}

// Sit out the pause between rounds, then start the next one. The one thing here
// that is not an animation: it draws nothing, it only lets the clock run out.
//
// One countdown for both pauses rather than two, because they are mutually
// exclusive and the length is fixed at the moment the pause begins - see
// simon_says_pause.
//
// Written as a >= rather than an early return on <, so that a pause of 0 - a real
// setting, meaning "ask again straight away" - does not read as a comparison of an
// unsigned against zero that can never be true.
static void simon_says_step_pause(void) {
    if (!simon_says_paused) return;

    if (simon_says_now - simon_says_pause_started >= simon_says_pause) {
        simon_says_paused       = false;
        simon_says_showing      = true;
        simon_says_show_started = simon_says_now;
    }
}

// Start the score print once the delay after the mistake is up. Like the pause, one
// of the things here that draws nothing: the drawing is rgb_print()'s job, called
// per chunk from simon_says() below.
//
// printTick is the shared counter from helpers/rgb_print.h, and it is shared with
// calculator.h and dirc.h as well as with anything else that has printed since the
// keyboard booted. Only one effect runs at a time so nothing can be reading it, but
// its value is not ours to inherit, hence the reset: without it a print starts part
// way through its own digit slots and shows nothing.
static void simon_says_step_print(void) {
    if (!simon_says_print_queued) return;

    if (simon_says_now - simon_says_print_started < SIMON_SAYS_PRINT_DELAY_MS) return;

    simon_says_print_queued  = false;
    simon_says_print_running = true;
    printTick                = 0;
}

// Declared here rather than only in the order they are defined below, because
// simon_says_step() calls all of them and reads best sitting next to the renderer
// that consumes what they fill in.
static void simon_says_step_input(void);
static void simon_says_step_pause(void);
static void simon_says_step_print(void);
static void simon_says_step_show(void);
static void simon_says_step_sweep(void);

// One frame of the game, in the order the frame needs them.
//
// The input is read before the animations are advanced on purpose. A judgement
// made this frame restarts the replay and starts a sweep, and both then get the
// rest of the frame to begin on: a correct round does not light the first key of
// its next sequence a frame late, and a mistake does not light its new single
// letter a frame after the red sweep it triggered.
static void simon_says_step(bool init) {
    simon_says_now  = timer_read32();
    simon_says_fade = reactive_fade_window(rgb_matrix_config.speed, SIMON_SAYS_FADE_MIN_MS, SIMON_SAYS_FADE_MAX_MS);

    // Everything dark to begin with; the two steps below light what they are
    // showing. An array rather than a per LED calculation because each chunk of a
    // frame only renders its own slice - see simon_says_age[].
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        simon_says_age[i] = SIMON_SAYS_AGE_OFF;
    }

    if (init) {
        // The first frame after this effect is selected. Drop a sweep and any
        // verdicts left over from whatever ran before, and deal a new game. It goes
        // straight into the replay, no penalty, because there is nothing to be
        // punished for yet - and starting in the replay is what makes the key
        // readings below safe to seed the press shadow with instead of being judged
        // as presses: a letter already under your finger when you switch over is not
        // a press, and one you press in the same frame is not either.
        simon_says_sweep = SIMON_SAYS_SWEEP_NONE;

        // Only here, and only ever here: a verdict is meant to survive the rest of
        // the round, so a mistake must not clear them. See simon_says_verdict_at[].
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            simon_says_verdict_at[i] = SIMON_SAYS_NO_TIME;
        }

        // A print left running by whatever ran before is dropped rather than
        // adopted: its buffer is ours and its digits mean nothing to this game, and
        // rgb_print() cannot be told to stop early other than by being asked again
        // until it says it is done.
        simon_says_print_queued  = false;
        simon_says_print_running = false;
        simon_says_print_count   = 0;

        // The shared generator behind random8_max() starts every boot from the
        // same fixed seed, so without this the game would open on the same letter
        // every time. This is the entropy hook it provides for exactly that.
        random16_add_entropy((uint16_t)simon_says_now);

        simon_says_deal();
        simon_says_showing      = true;
        simon_says_show_started = simon_says_now;
    }

    simon_says_step_input();

    // Before the replay, so that the frame the pause runs out on is the frame the
    // first key of the new sequence lights: a zero-length pause, set this same
    // frame by the judgement above, still gets a whole frame of replay started and
    // stepped rather than none at all.
    simon_says_step_pause();

    // After the input, so that a print delay of 0 armed by the judgement above still
    // starts its print on that same frame. The print is independent of the pause -
    // the two lengths are separate settings - so this is only about not making a
    // zero delay mean "next frame".
    simon_says_step_print();

    if (simon_says_showing) {
        simon_says_step_show();
    }

    simon_says_step_sweep();
}

// Watch the letters for a press and judge at most one of them.
//
// The shadow is refreshed for every letter first and only then is a press picked
// out of the frame, so a key cannot be counted twice however the two passes are
// ordered, and so a second key found in the same scan is already recorded before
// anything is judged. The first press in scan order is the one that counts; a
// second key genuinely down in the same scan is ambiguous either way.
//
// The shadow is refreshed in the replay phase too, and not just while waiting.
// That is what stops a key you happened to be holding when the replay ended from
// arriving as a fresh press the moment your turn starts.
static void simon_says_step_input(void) {
    uint8_t pressed = SIMON_SAYS_NO_LETTER;

    if (held_keys_switch_down(K_ENTER)) {
        simon_says_deal();
        simon_says_pause         = SIMON_SAYS_ROUND_COOLDOWN_MS;
        simon_says_paused        = true;
        simon_says_pause_started = simon_says_now;

        simon_says_verdict(K_ENTER, true);
        return;
    }

    for (uint8_t l = 0; l < SIMON_SAYS_LETTERS; l++) {
        bool down = held_keys_switch_down(simon_says_letters[l]);
        if (down && !simon_says_shadow[l]) pressed = l;
        simon_says_shadow[l] = down;
    }

    // Presses during the replay are ignored, and so is everything else the frame
    // after a judgement: a mistake or a finished round both deal a new sequence and
    // then pause, so the other key that came down alongside the judged one is
    // dropped with them rather than being answered against a sequence that has just
    // changed. The pause counts for the same reason and more strongly - the next
    // sequence is already dealt, so mashing through it would otherwise be answered
    // against a round the player has not been shown.
    //
    // A print running counts too, and would not otherwise: normally a print lands
    // well inside the new game pause and never gets this far, but the print delay
    // and the pause are independent settings, and a score scrolling across the board
    // with a keypress being judged underneath it is a keypress answered against a
    // sequence nobody can see.
    if (simon_says_showing || simon_says_paused || simon_says_print_running || pressed == SIMON_SAYS_NO_LETTER) return;

    simon_says_answer(pressed);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// Five owners, in order of who gets the LED.
static void simon_says_paint(uint8_t led) {
    // While a sweep is running it owns its four keys outright - even in the replay
    // phase, which is what lets a round be scored and its next sequence shown at
    // the same time. It comes first because a score key is never a letter, so it
    // cannot be the key the player just pressed either and the two never disagree.
    if (simon_says_sweep != SIMON_SAYS_SWEEP_NONE && simon_says_is_progress_key(led)) {
        uint32_t age = simon_says_age[led];
        if (age == SIMON_SAYS_AGE_OFF) {
            rgb_matrix_set_color(led, 0x00, 0x00, 0x00);
            return;
        }

        HSV hsv = simon_says_sweep_hsv();
        RGB rgb = hsv_to_rgb(SOLID_REACTIVE_SIMPLE_math(hsv, reactive_fade_offset(age, simon_says_fade)));
        rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
        return;
    }

    // Then any key that has been judged and whose fade has not run out, green or
    // red as it was judged, fading out on the same shared curve as the sweep - so
    // the speed slider moves it like it moves everything else, and it is one knob
    // for the whole effect rather than a duration of its own that nothing else
    // would answer to.
    //
    // Several of these can be lit at once, each on its own clock, which is the
    // point of keeping them per LED: a sequence entered quickly leaves a trail of
    // greens behind it rather than only the newest one.
    //
    // Ahead of the replay and of the pause because a judgement is newer than
    // either: a key just found right stays green even though the sequence it
    // belongs to is already being played again underneath, and a key just found
    // wrong stays red for the whole of the pause that follows rather than being
    // swallowed by the black of it.
    //
    // An LED with no live verdict falls through to whoever owns the key now, which
    // for a key still under your finger is the held key feedback.
    if (simon_says_verdict_at[led] != SIMON_SAYS_NO_TIME) {
        uint32_t age = simon_says_now - simon_says_verdict_at[led];
        if (reactive_fade_offset(age, simon_says_fade) != 255) {
            HSV hsv = simon_says_verdict_hsv(simon_says_verdict_good[led]);
            RGB rgb = hsv_to_rgb(SOLID_REACTIVE_SIMPLE_math(hsv, reactive_fade_offset(age, simon_says_fade)));
            rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
            return;
        }
    }

    // The replay owns everything that is not a score key, and the key it is
    // showing is the only one with an age.
    if (simon_says_showing) {
        if (simon_says_age[led] == SIMON_SAYS_AGE_OFF) {
            rgb_matrix_set_color(led, 0x00, 0x00, 0x00);
            return;
        }

        RGB rgb = hsv_to_rgb(map_colors(led));
        rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
        return;
    }

    // In the pause between rounds the letters are dark. It is not your turn, so the
    // held key feedback below has nothing to say to you, and a black board with four
    // keys sweeping it says the game is thinking rather than that it has stopped
    // listening. Note this is after the sweep: the score column keeps its answer
    // going for the whole pause, and falls to black with the rest of it once the
    // fade is done, since a score key here is simply not a letter.
    if (simon_says_paused) {
        rgb_matrix_set_color(led, 0x00, 0x00, 0x00);
        return;
    }

    // Otherwise it is your turn and the rest of the keymap takes the board back:
    // a key is lit for exactly as long as it is held, which is the "did that
    // register?" feedback helpers/held_keys.h exists to draw. A key that is not
    // being pressed has nothing lit on it, because the game has said its piece the
    // moment it judged the last one.
    held_keys_fade_paint(led, map_colors(led));
}

static bool simon_says(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // The effect is invoked once per RGB_MATRIX_LED_PROCESS_LIMIT chunk, so
    // params->iter == 0 is the first chunk of a frame. Everything that is a
    // property of the frame rather than of an LED happens there, once.
    if (params->iter == 0) {
        // Stepped before the input is read, because that read is the shared
        // held_keys state: held_keys_switch_down() only answers about this frame
        // once held_keys_step() has run.
        held_keys_step(params->init, held_keys_switch_down);
        simon_says_step(params->init);
    }

    // A print owns the whole strip, the same way calculator.h hands its answer the
    // whole strip, and for the same reason: a score is a thing you read, and it
    // cannot be read from a board that is also replaying a sequence or sweeping a
    // verdict. rgb_print() draws one chunk's worth per call and says when it is
    // done, which is what retires the print here.
    if (simon_says_print_running) {
        if (rgb_print(led_min, led_max, simon_says_print_buffer, simon_says_print_count, SIMON_SAYS_PRINT_KEY_MS, SIMON_SAYS_PRINT_GAP_MS, 0, simon_says_print_count)) {
            simon_says_print_running = false;
        }
        return rgb_matrix_check_finished_leds(led_max);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        simon_says_paint(i);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
