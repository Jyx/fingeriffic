#include <Bounce2.h>
#include <LiquidCrystal.h>

/*
 * ©2011 bildr
 * Released under the MIT License - Please reuse change and share
 * Using the easy stepper with your arduino
 * use rotate and/or rotate_deg to controll stepper motor
 * speed is any number from .01 -> 1 with 1 being fastest -
 * Slower Speed == Stronger movement
 */
#define STEPS_PER_REV 1600
#define MM_PER_REV 1.25

/* Buttons */
#define BTN_ENTER 8 /* FIXME: Should be 1 */
#define BTN_UP 6
#define BTN_DOWN 7
#define BTN_ESCAPE 10
#define BTN_PROTECT 13

/* Instantiate the Bounce objects */
Bounce db_enter = Bounce();
Bounce db_up = Bounce();
Bounce db_down = Bounce();
Bounce db_escape = Bounce();
Bounce db_protect = Bounce();

/* Stepper */
#define DIR_PIN 8
#define STEP_PIN 9

/* LCD */
#define LCD_RS 12
#define LCD_ENABLE 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

/* DEGUG */

#define DEBUG
#if defined(DEBUG)
#define DBG_PRINT(s) Serial.print(s)
#define DBG_PRINTLN(s) Serial.println(s)
#else
#define DBG_PRINT(s)
#define DBG_PRINTLN(s)
#endif

enum {
	MI_BLADE,
	MI_BLADE_CONF,
	MI_BOX_WIDTH,
	MI_BOX_WIDTH_CONF,
	MI_WOOD_WIDTH,
	MI_WOOD_WIDTH_CONF,
	MI_MOVE,
	MI_MOVE_CONF,
	MI_RESET,
	MI_RESET_CONF,
	MI_START,
	MI_START_INITIATED,
	MI_END
};

static unsigned char state;
static unsigned char state_last;
static unsigned char local_state;

static bool started;
static bool config_mode;

static bool btn_enter;
static bool btn_enter_last;
static bool btn_up;
static bool btn_up_last;
static bool btn_down;
static bool btn_down_last;
static bool btn_escape;
static bool btn_escape_last;
static bool btn_protect;
static bool btn_protect_last;

static float blade_width = 2.3;
static float box_width = 5.0;
static float wood_width = 100;
static unsigned int position;

static void init_buttons()
{
	btn_enter = false;
	btn_enter_last = false;
	btn_up = false;
	btn_up_last = false;
	btn_down = false;
	btn_down_last = false;
	btn_escape = false;
	btn_escape_last = false;
	btn_protect = false;
	btn_protect_last = false;
	local_state = false;
}

void setup()
{
	Serial.begin(9600);
	lcd.begin(16, 2);
	lcd.print("Initializing ...");

	pinMode(BTN_ENTER, INPUT);
	db_enter.attach(BTN_ENTER);
	db_enter.interval(5);

	pinMode(BTN_UP, INPUT);
	db_up.attach(BTN_UP);
	db_up.interval(5);

	pinMode(BTN_DOWN, INPUT);
	db_down.attach(BTN_DOWN);
	db_down.interval(5);

	pinMode(BTN_ESCAPE, INPUT);
	db_escape.attach(BTN_ESCAPE);
	db_escape.interval(5);

	pinMode(BTN_PROTECT, INPUT);
	db_protect.attach(BTN_PROTECT);
	db_protect.interval(5);

	/* FIXME: Update later on when connecting a stepper
	pinMode(DIR_PIN, OUTPUT);
	pinMode(STEP_PIN, OUTPUT);
	*/

	started = false;
	config_mode = false;
	state = MI_BLADE;
	position = 0;
}

static void read_buttons()
{
	db_enter.update();
	btn_enter = db_enter.read();

	db_up.update();
	btn_up = db_up.read();

	db_down.update();
	btn_down = db_down.read();

	db_escape.update();
	btn_escape = db_escape.read();

	db_protect.update();
	btn_protect = db_escape.read();
}

static unsigned int to_steps(float mm)
{
	float ratio = STEPS_PER_REV / MM_PER_REV;
	/* FIXME: Check for eventual casting and rounding problems */
	return (unsigned int)(ratio * mm);
}

static float to_mm(unsigned int steps) {
	float ratio = MM_PER_REV / STEPS_PER_REV;
	return ratio * steps;
}


static void update_menu()
{
	lcd.setCursor(0, 0);
	if (state != state_last)
		lcd.clear();

	switch (state) {
	case MI_BLADE:
	case MI_BLADE_CONF:
		lcd.print("Blade width");
		if (state == MI_BLADE_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(blade_width, 2);
		lcd.print("mm");
		break;

	case MI_BOX_WIDTH:
	case MI_BOX_WIDTH_CONF:
		lcd.print("Box joint width");
		if (state == MI_BOX_WIDTH_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(box_width, 2);
		lcd.print("mm");
		break;

	case MI_WOOD_WIDTH:
	case MI_WOOD_WIDTH_CONF:
		lcd.print("Wood width");
		if (state == MI_WOOD_WIDTH_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(wood_width, 2);
		lcd.print("mm");
		break;

	case MI_MOVE:
	case MI_MOVE_CONF:
		lcd.print("Move sled");
		if (state == MI_MOVE_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(position);
		lcd.print("steps, ");
		lcd.print(to_mm(position), 2);
		break;

	case MI_RESET_CONF:
	case MI_RESET:
		lcd.print("Reset sled pos");
		if (state == MI_RESET_CONF) {
			lcd.print(" (*)");
			lcd.setCursor(0, 1);
			lcd.print("... working ...");
		}
		break;

	case MI_START:
		lcd.print("Start?");
		if (state == MI_RESET_CONF) {
			lcd.print(" (*)");
			lcd.setCursor(0, 1);
			lcd.print("... working ...");
		}
		break;

	case MI_START_INITIATED:
		lcd.print("Running ...");
		break;
	}
}

/*
 * Rotate a specific number of degrees (negitive for reverse movement) speed is
 * any number from .01 -> 1 with 1 being fastest - Slower is stronger
 */
void rotate_deg(float deg, float speed) {
	int dir = (deg > 0) ? HIGH : LOW;
	digitalWrite(DIR_PIN, dir);

	int steps = abs(deg) * (1 / 0.225);
	float usDelay = (1 / speed) * 70;

	for (int i = 0; i < steps; i++) {
		digitalWrite(STEP_PIN, HIGH);
		delayMicroseconds(usDelay);

		digitalWrite(STEP_PIN, LOW);
		delayMicroseconds(usDelay);
	}
}

/*
 * Rotate a specific number of microsteps (8 microsteps per step) - (negitive
 * for reverse movement) speed is any number from .01 -> 1 with 1 being fastest
 * slower is stronger.
 */
void rotate(int steps, float speed) {
	int dir = (steps > 0) ? HIGH : LOW;
	steps = abs(steps);

	digitalWrite(DIR_PIN, dir);

	float usDelay = (1 / speed) * 70;

	for (int i = 0; i < steps; i++) {
		digitalWrite(STEP_PIN, HIGH);
		delayMicroseconds(usDelay);

		digitalWrite(STEP_PIN, LOW);
		delayMicroseconds(usDelay);
	}
}

void stepper_move() {
	/* rotate a specific number of degrees */
	rotate_deg(360, 1);
	delay(1000);

	rotate_deg(-360, .1);  /* reverse */
	delay(1000);


	/*
	 * rotate a specific number of microsteps (8 microsteps per step) a 200
	 * step stepper would take 1600 micro steps for one full revolution
	 */
	rotate(1600, .5);
	delay(1000);

	rotate(-1600, .25); /* reverse */
	delay(1000);
}


#if 0
static unsigned int calc_optimal_mm_width(unsigned int total_wood_height,
					  unsigned int box_width)
{
	/* FIXME: Check if this is useful or not */
	return total_wood_height / 10;
}
#endif

static void menu_up()
{
	state += 2;

	/* Check for wrapping */
	if (state >= MI_END)
		state = MI_BLADE;
}

static void menu_down()
{
	state -= 2;

	/* Check for wrapping */
	if (state >= MI_END)
		state = MI_END - 2;
}

static void handle_blade_conf()
{
	if (btn_up) {
		blade_width += 0.1;
	} else if (btn_down) {
		blade_width -= 0.1;
	} else if (btn_escape) {
		state = MI_BLADE;
	}

	if (blade_width < 0)
		blade_width = 0;
}

static void handle_box_conf()
{
	if (btn_up) {
		box_width += 0.1;
	} else if (btn_down) {
		box_width -= 0.1;
	} else if (btn_escape) {
		state = MI_BOX_WIDTH;
	}

	if (box_width < 0)
		box_width = 0;
}

static void handle_wood_conf()
{
	if (btn_up) {
		wood_width += 0.1;
	} else if (btn_down) {
		wood_width -= 0.1;
	} else if (btn_escape) {
		state = MI_WOOD_WIDTH;
	}

	if (wood_width < 0)
		wood_width = 0;
}

static void handle_move_conf()
{
	if (btn_up) {
		rotate(to_steps(1.0), 0.5);
		position++;
	} else if (btn_down) {
		rotate(-1 * to_steps(1.0), 0.5);
		position--;
	} else if (btn_escape) {
		state = MI_MOVE;
	}
}

static void handle_reset_conf()
{
	if (btn_enter) {
		rotate(-1 * position, .5);
	} else if (btn_escape) {
		state = MI_RESET;
	}
}

static void handle_start_init()
{
	if (btn_enter) {
		started = true;
	} else if (btn_escape) {
		started = false;
		state = MI_START;
	}
}

static void handle_state()
{
	if (config_mode) {
		switch (state) {
		case MI_BLADE_CONF:
			handle_blade_conf();
			break;

		case MI_BOX_WIDTH_CONF:
			handle_box_conf();
			break;

		case MI_WOOD_WIDTH_CONF:
			handle_wood_conf();
			break;

		case MI_MOVE_CONF:
			handle_move_conf();
			break;

		case MI_RESET_CONF:
			handle_reset_conf();
			break;

		case MI_START_INITIATED:
			handle_start_init();
			break;
		}
		if (btn_escape != btn_escape_last) {
			if (btn_escape) {
				config_mode = false;
			}
		}
	} else {
		if (btn_up != btn_up_last) {
			if (btn_up) {
				switch (state) {
				case MI_BLADE:
				case MI_BOX_WIDTH:
				case MI_WOOD_WIDTH:
				case MI_MOVE:
				case MI_RESET:
				case MI_START:
				menu_up();
				DBG_PRINTLN("up pressed");
				}
			} else {
				DBG_PRINTLN("up released");
			}
			btn_up_last = btn_up;
		} else if (btn_down != btn_down_last) {
			if (btn_down) {
				switch (state) {
				case MI_BLADE:
				case MI_BOX_WIDTH:
				case MI_WOOD_WIDTH:
				case MI_MOVE:
				case MI_RESET:
				case MI_START:
					menu_down();
				DBG_PRINTLN("down pressed");
				}
			} else {
				DBG_PRINTLN("down released");
			}
			btn_down_last = btn_down;
		} else if (btn_enter != btn_enter_last) {
			if (btn_enter) {
				/*
				 * We can do like this as long as the enum
				 * consists of menu, conf, menu, conf ..
				 */
				state += 1;
				config_mode = true;
				DBG_PRINTLN("enter pressed");
			} else {
				DBG_PRINTLN("enter released");
			}
			btn_enter_last = btn_enter;
		}
	}
	state_last = state;
}

static void handle_running()
{
	/* Check if we want to stop running */
	if (btn_escape != btn_escape_last) {
		if (btn_escape) {
			config_mode = false;
			started = false;
			state = MI_START;
		}
	}

	/*
	 * Check that the protect button is set before trying to run
	 * the stepper motor
	 */
	if (digitalRead(BTN_PROTECT) == 1) {
		// Run the
	} else {
		// Print an error message?
	}
}

void loop()
{
	read_buttons();

	if (started) {
		handle_running();
	} else {
		handle_state();
	}

	update_menu();

#if defined(DEBUG)
	if (state != local_state) {
		DBG_PRINT("State: ");
		DBG_PRINTLN(state);
	}

	local_state = state;
#endif
}
