// my_macos_menubar.m (or .mm)
#include <Cocoa/Cocoa.h>

#include "menu.h"

void DarwinMenuBar() {
	NSMenu *MenuBar = [[NSMenu alloc] init];
	[NSApp setMainMenu:MenuBar];

	NSMenuItem *AppMenuItem = [[NSMenuItem alloc] init];
	[MenuBar addItem:AppMenuItem];

	NSMenu *AppMenu = [[NSMenu alloc] initWithTitle:@"App"];
	
	NSString *AboutTitle = [NSString stringWithFormat:@"About %@", 
						   [[NSProcessInfo processInfo] processName]];
	
	NSMenuItem *AboutItem = [[NSMenuItem alloc] initWithTitle:AboutTitle 
												action:@selector(orderFrontStandardAboutPanel:) 
												keyEquivalent:@""];
	[AppMenu addItem:AboutItem];
	
	[AppMenu addItem:[NSMenuItem separatorItem]];
	
	NSString *QuitTitle = [NSString stringWithFormat:@"Quit %@", 
						  [[NSProcessInfo processInfo] processName]];
						  
	NSMenuItem *QuitItem = [[NSMenuItem alloc] initWithTitle:QuitTitle
											   action:@selector(terminate:) 
											   keyEquivalent:@"q"];
	[AppMenu addItem:QuitItem];
	
	[AppMenuItem setSubmenu:AppMenu];
}