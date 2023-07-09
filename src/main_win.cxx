#if defined(_WIN32)
#define WIN32
#endif
#define MINIAUDIO_IMPLEMENTATION

#include "miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

#if defined(_WIN32)
// Sleep()
#include <synchapi.h>
#else
// sleep()
#include <unistd.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Button.H>
#include <FL/filename.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_message.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Pixmap.H>
// Pixmaps:
#include "xhk.xpm"
#include "recbtn.xpm"
#include "stopbtn.xpm"

int rec_stopped = 0;
int rec_success = 0;
int time_elapsed = 0;
static Fl_Output* time_out;

static Fl_Pixmap image_xhk((const char**)xhk_xpm);
static Fl_Pixmap image_rec((const char**)recbtn_xpm);
static Fl_Pixmap image_stop((const char**)stopbtn_xpm);

static void reset_cb() {
	/*
	 * Callback function for Reset Menu button
	 * Unless all global variables are
	 * redefined to zero, a new recording
	 * session can't be started.
	 */	
	time_elapsed = 0;
	rec_success = 0;
	rec_stopped = 0;
}

static void about(const std::string& name, const std::string& title, const std::string& description, const std::string& version, const std::string& copyright) {
	/*
	 * Helper function for about button callback
	 */
	fl_message_icon()->label("@");
	fl_message_title(title.c_str());
	fl_choice("%s", nullptr, fl_ok, nullptr, (name + "\n" + version + "\n \n" + copyright + "\n" + description + "\n ").c_str());
}

static void about_cb() {
	/*
	 * Callback function for About button
	 */
	about("Sound Recorder", "About", "\nA simple, barebones sound recorder\nfor XHaskell\n\n(Press 'Reset' after each session\nto record again.)", "1.0.0", "Copyright (c) 2023 searemind.\nAll rights reserved.");
}

// Audio recording logic from miniaudio simple_capture.c
static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
	ma_encoder* pEncoder = (ma_encoder*)pDevice->pUserData;
	MA_ASSERT(pEncoder != NULL);
	ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);
	(void)pOutput;
}

static void stop_cb(Fl_Widget* w, void*) {
	/*
	 * Callback function for Stop button
	 * Displays an alert message after
	 * successful recording session.
	 */
	rec_stopped = 1;
	if (rec_success == 1) {
		fl_message_title("Success");
		fl_message("Recording has been saved.");
	}
}

// Audio recording logic from miniaudio simple_capture.c
static void minaud_rec(const char* result_file) {
	ma_result result;
	ma_encoder_config encoderConfig;
	ma_encoder encoder;
	ma_device_config deviceConfig;
	ma_device device;

	encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);
	if (ma_encoder_init_file(result_file, &encoderConfig, &encoder) != MA_SUCCESS) {
		printf("Failed to initialize output file.\n");
	}
	deviceConfig = ma_device_config_init(ma_device_type_capture);
	deviceConfig.capture.format = encoder.config.format;
	deviceConfig.capture.channels = encoder.config.channels;
	deviceConfig.sampleRate = encoder.config.sampleRate;
	deviceConfig.dataCallback = data_callback;
	deviceConfig.pUserData = &encoder;
	result = ma_device_init(NULL, &deviceConfig, &device);
	if (result != MA_SUCCESS) {
		printf("Failed to initialize capture device.\n");
	}
	result = ma_device_start(&device);
	if (result != MA_SUCCESS) {
		ma_device_uninit(&device);
		printf("Failed to start device.\n");
	}
	printf("Recording...\n");
	/*
	 * Infinite loop which determines length
	 * of recording; sets timer value in global
	 * variable to display in Fl_Output widget.
	 */
	while (1) {
		rec_success = 1;
#if defined(_WIN32)
		Sleep(1000);
#else
		sleep(1);
#endif
		time_elapsed += 1;
		if (rec_stopped == 1) {
			break;
		}
	}
	/*
	 * Set rec_success = 0 so that
	 * pressing Stop button after recording
	 * does not cause creation of pop-up.
	*/
	rec_success = 0;
	ma_device_uninit(&device);
	ma_encoder_uninit(&encoder);
}

static void record_cb(Fl_Widget* w, void*) {
	/*
	 * Callback function for Record button
	 * Creates file chooser window, for
	 * creating new file: works for both
	 * Windows and Posix; then calls the audio
	 * recording logic in a separate detached
	 * thread.
	 */
	Fl_File_Chooser* saveFileDialog = new Fl_File_Chooser("", "", Fl_File_Chooser::CREATE, "Choose File");
	saveFileDialog->filter("All Files (*)");

#if defined (_WIN32)
	saveFileDialog->directory(((std::string(getenv("HOMEDRIVE"))) + (std::string(getenv("HOMEPATH")) + "\\Desktop")).c_str());
#else
	saveFileDialog->directory((std::string(getenv("HOME"))).c_str());
#endif
	saveFileDialog->value("recording.wav");
	saveFileDialog->show();
	/*
	 * while loop is used to prevent choice
	 * being alloted right after show() is invoked,
	 * causing the value to be the predefined
	 * one.
	 */
	while (saveFileDialog->shown()) Fl::wait();
	const char* result_file = saveFileDialog->value();
	if (saveFileDialog->value() != NULL) {
		printf("%s\n", result_file);
		std::thread rec_t(minaud_rec, result_file);
		rec_t.detach();
	} else printf("Cancelled\n");
}

static void menubar_cb(Fl_Widget *w, void*) {
	/*
	 * Callback function for Menu Bar
	 */
	Fl_Menu_Bar *bar = (Fl_Menu_Bar*)w;
	const Fl_Menu_Item *item = bar->mvalue();
	if (strcmp(item->label(), "&Reset") == 0) {
		reset_cb();
	}
	if (strcmp(item->label(), "&About") == 0) {
		about_cb();
	}
	if (strcmp(item->label(), "&Quit") == 0) {
		w->window()->hide();
	}
}

static void timeout_cb(void*) {
	/* Special Callback function for
	 * timer, redraws value in Fl_Output
	 * recursively
	 */
	time_out->value((std::string("Time (sec): ") + (std::to_string(time_elapsed))).c_str());
	Fl::redraw();
	Fl::repeat_timeout(1, timeout_cb);
}

static void window_center_on_screen(Fl_Window* win) {
	/*
	 * Logic to center window on screen
	 */
	int X, Y, W, H;
	Fl::screen_xywh(X, Y, W, H);
	win->position(W / 2 - win->w() / 2, H / 2 - win->h() / 2);
}

int main(int argc, char **argv) {
	/*
	 * Program entry-point.
	 */
	Fl::scheme("gtk+");
	Fl_Window *window = new Fl_Window(250,150, "Recorder");
	
	Fl_Menu_Bar *menu = new Fl_Menu_Bar(0,0,250,25);
	{
		menu->add("&Reset", "^r", menubar_cb);
		menu->add("&Quit", "^w", menubar_cb);
		menu->add("&About", 0, menubar_cb);
	}
	// XHaskell logo display
	Fl_Box* image_box = new Fl_Box(5, 30, 60, 55);
	image_box->image(image_xhk);

	Fl_Box* title_label_box = new Fl_Box(70, 30, 300, 25, "Recorder");
	title_label_box->labelfont(1);
	title_label_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	
	time_out = new Fl_Output(70, 55, 120, 25);

	Fl_Button* record_button = new Fl_Button(10, 100, 60, 40, "Record");
	record_button->image(image_rec);
	record_button->callback(record_cb);

	Fl_Button* stop_button = new Fl_Button(180, 100, 60, 40, "Stop");
	stop_button->image(image_stop);
	stop_button->callback(stop_cb);
	
	window->end();

	/*
	 * Set app window (and taskbar) icon,
	 * label and identifier
	 */
	window->iconlabel("Recorder");
	Fl_RGB_Image myapp_icon(&image_xhk, Fl_Color(0));
	window->icon(&myapp_icon);
	window->xclass("Recorder");

	window_center_on_screen(window);
	window->show(argc, argv);
	// Starts timeout_cb
	Fl::add_timeout(0.009, timeout_cb);
	return Fl::run();
}
