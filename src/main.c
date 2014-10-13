#include "pebble.h"
#include "common.h"

static Window* window;
static GBitmap *background_image_container;

static Layer *minute_display_layer;
static Layer *hour_display_layer;
static Layer *center_display_layer;
static Layer *second_display_layer;
static TextLayer *date_layer , *dig_time_layer, *steps_layer;
static char date_text[] = "Wed 13.11.14 ";
static char timeBuffer[] = "00:00.";
static bool bt_ok = false;
static uint8_t battery_level;
static bool battery_plugged;

static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static GBitmap *icon_bt;

static Layer *battery_layer;
static Layer *bt_layer;

bool g_conserve = false;
bool showSeconds = false;
static Layer *background_layer;
static Layer *window_layer;

const GPathInfo MINUTE_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
				{ 4, 15 }, { 4, -70 }, { -4, -70 }, } };

const GPathInfo HOUR_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
				{ 4, 15 }, { 4, -50 }, { -4, -50 }, } };

static GPath *hour_hand_path;
static GPath *minute_hand_path;

static AppTimer *timer_handle;
#define COOKIE_MY_TIMER 1
static int my_cookie = COOKIE_MY_TIMER;
#define ANIM_IDLE 0
#define ANIM_START 1
#define ANIM_HOURS 2
#define ANIM_MINUTES 3
#define ANIM_SECONDS 4
#define ANIM_DONE 5
int init_anim = ANIM_DONE;
int32_t second_angle_anim = 0;
unsigned int minute_angle_anim = 0;
unsigned int hour_angle_anim = 0;
/*
 * Variables for Step Counting
 */
// Total Steps (TS)
#define TS 1
// Total Steps Default (TSD)
#define TSD 1
 
// Timer used to determine next step check
static AppTimer *timer;

// interval to check for next step (in ms)
const int ACCEL_STEP_MS = 475;
// value to auto adjust step acceptance 
const int PED_ADJUST = 2;
// steps required per calorie
const int STEPS_PER_CALORIE = 22;
// value by which step goal is incremented
const int STEP_INCREMENT = 50;
// values for max/min number of calibration options 
const int MAX_CALIBRATION_SETTINGS = 3;
const int MIN_CALIBRATION_SETTINGS = 1;

int X_DELTA = 75;
int Y_DELTA, Z_DELTA = 205;
int YZ_DELTA_MIN = 175;
int YZ_DELTA_MAX = 205; 
int X_DELTA_TEMP, Y_DELTA_TEMP, Z_DELTA_TEMP = 0;
int lastX, lastY, lastZ, currX, currY, currZ = 0;
int sensitivity = 1;

long stepGoal = 8000;
long pedometerCount = 0;
long caloriesBurned = 0;
long tempTotal = 0;

bool did_pebble_vibrate = false;
bool validX, validY, validZ = false;
bool SID;
bool isDark;
bool startedSession = false;


// Strings used to display theme and calibration options
char *theme;
char *cal = "Regular Sensitivity";
char *locale = "de_DE";

// stores total steps since app install
static long totalSteps = TSD;
int32_t lastHour = 0;

// configuration values
bool usePowerSaving = true;
bool showSteps = true;

#define SHOW_STEPS 0

void handle_timer(void* vdata) {

	int *data = (int *) vdata;

	if (*data == my_cookie) {
		if (init_anim == ANIM_START) {
			init_anim = ANIM_HOURS;
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_HOURS) {
			layer_mark_dirty(hour_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_MINUTES) {
			layer_mark_dirty(minute_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_SECONDS) {
			layer_mark_dirty(second_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		}
	}

}

void second_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	int32_t second_angle = t->tm_sec * (0xffff / 60);
	int second_hand_length = 70;
	GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
	GPoint second = GPoint(center.x, center.y - second_hand_length);

	if (init_anim < ANIM_SECONDS) {
		second = GPoint(center.x, center.y - 70);
	} else if (init_anim == ANIM_SECONDS) {
		second_angle_anim += 0xffff / 60;
		if (second_angle_anim >= second_angle) {
			init_anim = ANIM_DONE;
			second =
					GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
							center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
		} else {
			second =
					GPoint(center.x + second_hand_length * sin_lookup(second_angle_anim)/0xffff,
							center.y + (-second_hand_length) * cos_lookup(second_angle_anim)/0xffff);
		}
	} else {
		second =
				GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
						center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
	}

	graphics_context_set_stroke_color(ctx, GColorWhite);

	graphics_draw_line(ctx, center, second);
}

void center_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, 4);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, center, 3);
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	unsigned int angle = t->tm_min * 6 + t->tm_sec / 10;

	if (init_anim < ANIM_MINUTES) {
		angle = 0;
	} else if (init_anim == ANIM_MINUTES) {
		minute_angle_anim += 6;
		if (minute_angle_anim >= angle) {
			init_anim = ANIM_SECONDS;
		} else {
			angle = minute_angle_anim;
		}
	}

	gpath_rotate_to(minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
  
  if (lastHour > t->tm_hour){
    pedometerCount = 0;
  }
	unsigned int angle = t->tm_hour * 30 + t->tm_min / 2;

	if (init_anim < ANIM_HOURS) {
		angle = 0;
	} else if (init_anim == ANIM_HOURS) {
		if (hour_angle_anim == 0 && t->tm_hour >= 12) {
			hour_angle_anim = 360;
		}
		hour_angle_anim += 6;
		if (hour_angle_anim >= angle) {
			init_anim = ANIM_MINUTES;
		} else {
			angle = hour_angle_anim;
		}
    
    lastHour = t->tm_hour;
	}

	gpath_rotate_to(hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);
}

void draw_date() {

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
  
  if (strcmp(locale, "de_DE")==0){
	  strftime(date_text, sizeof(date_text), "%a %d.%m.%y", t);
  }else{
    strftime(date_text, sizeof(date_text), "%a %m/%d/%y", t);
  }

	text_layer_set_text(date_layer, date_text);
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {

  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  if (!battery_plugged) {
    graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, 24, 12));
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(7, 4, (uint8_t)((battery_level / 100.0) * 11.0), 4), 0, GCornerNone);
  } else {
    graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, 24, 12));
  }
}



void battery_state_handler(BatteryChargeState charge) {
	battery_level = charge.charge_percent;
	battery_plugged = charge.is_plugged;
	layer_mark_dirty(battery_layer);
	if (!battery_plugged && battery_level < 20)
		conserve_power(true);
	//else
		//conserve_power(false);
}

/*
 * Bluetooth icon callback handler
 */
void bt_layer_update_callback(Layer *layer, GContext *ctx) {
  if (bt_ok)
  	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  else
  	graphics_context_set_compositing_mode(ctx, GCompOpClear);
  graphics_draw_bitmap_in_rect(ctx, icon_bt, GRect(0, 0, 9, 12));
}

void bt_connection_handler(bool connected) {
	bt_ok = connected;
	layer_mark_dirty(bt_layer);
}

void draw_background_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	graphics_draw_bitmap_in_rect(ctx, background_image_container,
			GRECT_FULL_WINDOW);
}

void init() {

	// Window
	window = window_create();
	window_stack_push(window, true /* Animated */);
	window_layer = window_get_root_layer(window);

	// Background image
	background_image_container = gbitmap_create_with_resource(
			RESOURCE_ID_IMAGE_BACKGROUND);
	background_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(background_layer, &draw_background_callback);
	layer_add_child(window_layer, background_layer);

	// Date setup
	date_layer = text_layer_create(GRect(27, 100, 90, 21));
	text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(date_layer));
	
	//Digital time
	dig_time_layer = text_layer_create(GRect(27, 120, 90, 21));
	text_layer_set_text_color(dig_time_layer, GColorWhite);
	text_layer_set_text_alignment(dig_time_layer, GTextAlignmentCenter);
	text_layer_set_background_color(dig_time_layer, GColorClear);
	text_layer_set_font(dig_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(dig_time_layer));

	draw_date();
  
  // Steps setup
	steps_layer = text_layer_create(GRect(27, 22, 90, 21));
	text_layer_set_text_color(steps_layer, GColorWhite);
	text_layer_set_text_alignment(steps_layer, GTextAlignmentCenter);
	text_layer_set_background_color(steps_layer, GColorClear);
	text_layer_set_font(steps_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(steps_layer));
  
  static char buf[] = "123456890abcdefghijkl";
  snprintf(buf, sizeof(buf), "%ld", pedometerCount);
	text_layer_set_text(steps_layer, buf);

	// Status setup
	icon_battery = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_ICON);
	icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);
	icon_bt = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH);

	BatteryChargeState initial = battery_state_service_peek();
	battery_level = initial.charge_percent;
	battery_plugged = initial.is_plugged;
	battery_layer = layer_create(GRect(50,56,24,12)); //24*12
	layer_set_update_proc(battery_layer, &battery_layer_update_callback);
	layer_add_child(window_layer, battery_layer);


	bt_ok = bluetooth_connection_service_peek();
	bt_layer = layer_create(GRect(83,56,9,12)); //9*12
	layer_set_update_proc(bt_layer, &bt_layer_update_callback);
	layer_add_child(window_layer, bt_layer);

  
  
	// Hands setup
	hour_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(hour_display_layer,
			&hour_display_layer_update_callback);
	layer_add_child(window_layer, hour_display_layer);

	hour_hand_path = gpath_create(&HOUR_HAND_PATH_POINTS);
	gpath_move_to(hour_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	minute_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(minute_display_layer,
			&minute_display_layer_update_callback);
	layer_add_child(window_layer, minute_display_layer);

	minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
	gpath_move_to(minute_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	center_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(center_display_layer,
			&center_display_layer_update_callback);
	layer_add_child(window_layer, center_display_layer);

	second_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(second_display_layer,
			&second_display_layer_update_callback);
	layer_add_child(window_layer, second_display_layer);
  
  //Power consumption
  conserve_power(true);
  
  
}



void handle_tick(struct tm *tick_time, TimeUnits units_changed) {

	if (init_anim == ANIM_IDLE) {
		init_anim = ANIM_START;
		timer_handle = app_timer_register(50 /* milliseconds */, &handle_timer,
				&my_cookie);
	} else if (init_anim == ANIM_DONE) {
		if (tick_time->tm_sec % 10 == 0) {
			layer_mark_dirty(minute_display_layer);

			if (tick_time->tm_sec == 0) {
				if (tick_time->tm_min % 2 == 0) {
					layer_mark_dirty(hour_display_layer);
					if (tick_time->tm_min == 0 && tick_time->tm_hour == 0) {
						draw_date();
					}
				}
			}
		}

		layer_mark_dirty(second_display_layer);
	}
	if(clock_is_24h_style()){
			strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", tick_time);
		}
		else{
			strftime(timeBuffer,sizeof(timeBuffer),"%I:%M", tick_time);
		}
	text_layer_set_text(dig_time_layer, timeBuffer);
}

/*
 * Conserve Power if Battery is low 
 */
void conserve_power(bool conserve) {
	if (conserve == g_conserve)
		return;
	g_conserve = conserve;
	if (conserve) {
		tick_timer_service_unsubscribe();
		tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
		layer_set_hidden(second_display_layer, true);
	} else if (showSeconds) {
		tick_timer_service_unsubscribe();
		tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
		layer_set_hidden(second_display_layer, false);
	}
}

/*
 * Step Counter (Code basicly from https://github.com/jathusanT/pebble_pedometer)
 */



void autoCorrectZ(){
	if (Z_DELTA > YZ_DELTA_MAX){
		Z_DELTA = YZ_DELTA_MAX; 
	} else if (Z_DELTA < YZ_DELTA_MIN){
		Z_DELTA = YZ_DELTA_MIN;
	}
}

void autoCorrectY(){
	if (Y_DELTA > YZ_DELTA_MAX){
		Y_DELTA = YZ_DELTA_MAX; 
	} else if (Y_DELTA < YZ_DELTA_MIN){
		Y_DELTA = YZ_DELTA_MIN;
	}
}

void pedometer_update() {
	if (startedSession) {
		X_DELTA_TEMP = abs(abs(currX) - abs(lastX));
		if (X_DELTA_TEMP >= X_DELTA) {
			validX = true;
		}
		Y_DELTA_TEMP = abs(abs(currY) - abs(lastY));
		if (Y_DELTA_TEMP >= Y_DELTA) {
			validY = true;
			if (Y_DELTA_TEMP - Y_DELTA > 200){
				autoCorrectY();
				Y_DELTA = (Y_DELTA < YZ_DELTA_MAX) ? Y_DELTA + PED_ADJUST : Y_DELTA;
			} else if (Y_DELTA - Y_DELTA_TEMP > 175){
				autoCorrectY();
				Y_DELTA = (Y_DELTA > YZ_DELTA_MIN) ? Y_DELTA - PED_ADJUST : Y_DELTA;
			}
		}
		Z_DELTA_TEMP = abs(abs(currZ) - abs(lastZ));
		if (Z_DELTA_TEMP >= Z_DELTA) {
			validZ = true;
			if (Z_DELTA_TEMP - Z_DELTA > 200){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			} else if (Z_DELTA - Z_DELTA_TEMP > 175){
				autoCorrectZ();
				Z_DELTA = (Z_DELTA < YZ_DELTA_MAX) ? Z_DELTA + PED_ADJUST : Z_DELTA;
			}
		}
	} else {
		startedSession = true;
	}
}

void resetUpdate() {
	lastX = currX;
	lastY = currY;
	lastZ = currZ;
	validX = false;
	validY = false;
	validZ = false;
}

void update_ui_callback() {
	if ((validX && validY && !did_pebble_vibrate) || (validX && validZ && !did_pebble_vibrate)) {
		pedometerCount++;
		//tempTotal++;

		//caloriesBurned = (int) (pedometerCount / STEPS_PER_CALORIE);
		//static char calBuf[] = "123456890abcdefghijkl";
		//snprintf(calBuf, sizeof(calBuf), "%ld Calories", caloriesBurned);
		//text_layer_set_text(calories, calBuf);

    // steps
		static char buf[] = "123456890abcdefghijkl";
		snprintf(buf, sizeof(buf), "%ld", pedometerCount);
		text_layer_set_text(steps_layer, buf);

    // total steps
		//static char buf2[] = "123456890abcdefghijkl";
		//snprintf(buf2, sizeof(buf2), "%ld in Total", tempTotal);
		//menu_items[2].subtitle = buf2;
    
    // burned calories
		/*static char buf3[] = "1234567890abcdefg";
		snprintf(buf3, sizeof(buf3), "%ld Burned",
				(long) (tempTotal / STEPS_PER_CALORIE));
		menu_items[3].subtitle = buf3;
    */
		//layer_mark_dirty(window_get_root_layer(pedometer));
		//layer_mark_dirty(window_get_root_layer(menu_window));

		if (stepGoal > 0 && pedometerCount == stepGoal) {
			vibes_long_pulse();
			//window_set_window_handlers(window, (WindowHandlers ) { .load =
			//				window_load, .unload = window_unload, });
			//window_stack_push(window, true);
		}
	}

	resetUpdate();
}

void accel_data_handler(AccelData *accel_data, uint32_t num_samples) {
  // Process 9 events - every 1 second
  uint32_t i;

 			for (i=0;i<num_samples/3;i++){
          uint32_t appo = i*3; 
          AccelData accel = accel_data[appo];
          if (!startedSession) {
        		lastX = accel.x;
        		lastY = accel.y;
        		lastZ = accel.z;
        	} else {
        		currX = (accel_data[appo].x+accel_data[appo+1].x+accel_data[appo+2].x)/3;
        		currY = (accel_data[appo].y+accel_data[appo+1].y+accel_data[appo+2].y)/3;//accel.y;
        		currZ = (accel_data[appo].z+accel_data[appo+1].z+accel_data[appo+2].z)/3;//accel.z;
        	}
        	
        	did_pebble_vibrate = accel.did_vibrate;
        
        	pedometer_update();
					/*Zm=Zm+abs(accel_data[appo].z);

					Ym=Ym+abs(accel_data[appo].y);

					Xm=Xm+abs(accel_data[appo].x);
          */
  			}
  update_ui_callback();
}

static void timer_callback(void *data) {
	AccelData accel = (AccelData ) { .x = 0, .y = 0, .z = 0 };
	accel_service_peek(&accel);

	if (!startedSession) {
		lastX = accel.x;
		lastY = accel.y;
		lastZ = accel.z;
	} else {
		currX = accel.x;
		currY = accel.y;
		currZ = accel.z;
	}
	
	did_pebble_vibrate = accel.did_vibrate;

	pedometer_update();
	update_ui_callback();

	//layer_mark_dirty(window_get_root_layer(pedometer));
	timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

void update_from_settings(){
  if (showSteps){
    accel_data_service_subscribe(9, accel_data_handler);
    accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
    static char buf[] = "123456890abcdefghijkl";
    snprintf(buf, sizeof(buf), "%ld", pedometerCount);
	  text_layer_set_text(steps_layer, buf);
  }else{
    accel_data_service_unsubscribe();
    text_layer_set_text(steps_layer,"");
  }
}

/*
 * Handels User Taps on the Pebble
 */
void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  if (axis == ACCEL_AXIS_Z){
    text_layer_set_text(steps_layer, "TAP :)");
  }
}

/*
 * Handle AppMessages from Pebble JS
 */
static void in_recv_handler(DictionaryIterator *iterator, void *context)
{
  //Get Tuple
  Tuple *t = dict_read_first(iterator);
  if(t)
  {
    switch(t->key)
    {
    case SHOW_STEPS:
      //It's the KEY_INVERT key
      if(strcmp(t->value->cstring, "on") == 0)
      {
        //Set and save as inverted
        showSteps = true;
 
        //pedometerCount = persist_exists(TS) ? persist_read_int(TS) : TSD;
        pedometerCount = 0;
	      text_layer_set_text(steps_layer, "0");
        
        persist_write_bool(SHOW_STEPS, true);
      }
      else if(strcmp(t->value->cstring, "off") == 0)
      {
        //Set and save as not inverted
        showSteps = false;
        
        
        persist_write_bool(SHOW_STEPS, false);
      }
      break;
    /*  
    case STEPS_UPDATE_INTERVALL:
      if(strcmp(t->value->cstring, "minute") == 0)
      {
        //Set and save as inverted
         
        
        persist_write_int(STEPS_UPDATE_INTERVALL, true);
      }
      else if(strcmp(t->value->cstring, "second") == 0)
      {
        //Set and save as not inverted
        accel_data_service_unsubscribe();
        text_layer_set_text(steps_layer,"");
        
        persist_write_int(STEPS_UPDATE_INTERVALL, false);
      }
      break;
      */
    }
  }
  update_from_settings();
}

/*
 * deinit
 */
void deinit() {
  //totalSteps += pedometerCount;
	persist_write_int(TS, pedometerCount); // save steps on exit
	window_destroy(window);
	gbitmap_destroy(background_image_container);
	gbitmap_destroy(icon_battery);
	gbitmap_destroy(icon_battery_charge);
	gbitmap_destroy(icon_bt);
	text_layer_destroy(date_layer);
	text_layer_destroy(dig_time_layer);
  text_layer_destroy(steps_layer);
	layer_destroy(minute_display_layer);
	layer_destroy(hour_display_layer);
	layer_destroy(center_display_layer);
	layer_destroy(second_display_layer);
	layer_destroy(battery_layer);
	layer_destroy(bt_layer);
	layer_destroy(background_layer);
	gpath_destroy(hour_hand_path);
	gpath_destroy(minute_hand_path);
  accel_data_service_unsubscribe();
}

/*
 * Main - or main as it is known
 */
int main(void) {
	init();
  
	tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
  //timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
	bluetooth_connection_service_subscribe(&bt_connection_handler);
	battery_state_service_subscribe	(&battery_state_handler);
  
  // register for acellerometer events
  accel_data_service_subscribe(9, accel_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  //accel_tap_service_subscribe(accel_tap_handler); // if tap guestures are needed...
  
  // enable AppMessage to communicate with Phone and Pebble JS
  app_message_register_inbox_received((AppMessageInboxReceived) in_recv_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  //Get saved data...
  pedometerCount = persist_exists(TS) ? persist_read_int(TS) : TSD;
  showSteps = persist_exists(SHOW_STEPS) ? persist_read_bool(SHOW_STEPS) : true ;
	
  update_from_settings();
  
  app_event_loop();
	deinit();
}


