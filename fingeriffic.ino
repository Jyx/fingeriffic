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
#define BTN_ENTER 1
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
static bool started;
static bool config_mode;

static float blade_width = 2.3;
static float box_width = 5.0;
static float wood_width = 100;

void setup()
{
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

	pinMode(DIR_PIN, OUTPUT);
	pinMode(STEP_PIN, OUTPUT);

	started = false;
	config_mode = false;
	state = MI_BLADE;
}

static void update_bouncers()
{
	db_enter.update();
	db_up.update();
	db_down.update();
	db_escape.update();
	db_protect.update();
}

static void update_menu()
{
	lcd.setCursor(0, 0);

	switch (state) {
	case MI_BLADE:
	case MI_BLADE_CONF:
		lcd.print("Blade width:");
		if (state == MI_BLADE_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(blade_width, 2);
		lcd.print("mm");
		break;

	case MI_BOX_WIDTH:
	case MI_BOX_WIDTH_CONF:
		lcd.print("Box joint width:");
		if (state == MI_BOX_WIDTH_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(box_width, 2);
		lcd.print("mm");
		break;

	case MI_WOOD_WIDTH:
	case MI_WOOD_WIDTH_CONF:
		lcd.print("Wood width:");
		if (state == MI_WOOD_WIDTH_CONF)
			lcd.print(" (*)");

		lcd.setCursor(0, 1);
		lcd.print(wood_width, 2);
		lcd.print("mm");
		break;

	case MI_MOVE:
	case MI_MOVE_CONF:
		lcd.print("Moving to blade:");
		lcd.setCursor(0, 1);
		lcd.print("xxx mm");
		break;

	case MI_RESET_CONF:
	case MI_RESET:
		lcd.setCursor(0, 1);
		lcd.print("Reset");
		if (state == MI_RESET_CONF)
			lcd.print(" (*)");
		lcd.print("Moving to Zero ...");
		break;

	case MI_START:
		break;
	}
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

/*
 * rotate a specific number of degrees (negitive for reverse movement) speed is
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
 * rotate a specific number of microsteps (8 microsteps per step) - (negitive *
 * for reverse movement) speed is any number from .01 -> 1 with 1 being fastest  * slower is stronger.
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
	if (state > MI_END)
		state = MI_BLADE;
}

static void menu_down()
{
	state -= 2;

	/* Check for wrapping */
	if (state > MI_END)
		state = MI_END - 1;
}

static void handle_blade_conf()
{
	if (db_up.read() == HIGH) {
		blade_width += 0.1;
	} else if (db_down.read() == HIGH) {
		blade_width -= 0.1;
	} else if (db_escape.read() == HIGH) {
		state = MI_BLADE;
	}
	delay(20);
}

static void handle_box_conf()
{
	if (db_up.read() == HIGH) {
		box_width += 0.1;
	} else if (db_down.read() == HIGH) {
		box_width -= 0.1;
	} else if (db_escape.read() == HIGH) {
		state = MI_BOX_WIDTH;
	}
	delay(20);
}

static void handle_wood_conf()
{
	if (db_up.read() == HIGH) {
		wood_width += 0.1;
	} else if (db_down.read() == HIGH) {
		wood_width -= 0.1;
	} else if (db_escape.read() == HIGH) {
		state = MI_WOOD_WIDTH;
	}
	delay(20);
}

static void handle_move_conf()
{
	if (db_up.read() == HIGH) {
		rotate(to_steps(1.0), 0.5);
	} else if (db_down.read() == HIGH) {
		rotate(-1 * to_steps(1.0), 0.5);
	} else if (db_escape.read() == HIGH) {
		state = MI_MOVE;
	}
	delay(20);
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
		case MI_START_INITIATED:
			break;
		}
	} else {
		if (db_up.read() == HIGH) {
			switch (state) {
			case MI_BLADE:
			case MI_BOX_WIDTH:
			case MI_WOOD_WIDTH:
			case MI_MOVE:
			case MI_RESET:
			case MI_START:
				menu_up();
			}
		} else if (db_down.read() == HIGH) {
			switch (state) {
			case MI_BLADE:
			case MI_BOX_WIDTH:
			case MI_WOOD_WIDTH:
			case MI_MOVE:
			case MI_RESET:
			case MI_START:
				menu_down();
			}
		} else if (db_enter.read() == HIGH) {
			/*
			 * We can do like this as long as the enum consists of
			 * menu, conf, menu, conf ..
			 */
			state += 1;
			config_mode = true;
		}
	}
}

void loop()
{
	update_bouncers();
	handle_state();
	update_menu();

	if (started) {
		/*
		 * Check that the protect button is set before trying to run the
		 * stepper motor
		 */
		if (digitalRead(BTN_PROTECT) == 1) {
			// Run the
		} else {
			// Print an error message?
		}
	} else {
		// Configure ...
	}
}
