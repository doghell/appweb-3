//
//  appweb_iphoneAppDelegate.m
//  appweb-iphone
//
//  Created by Michael O'Brien on 5/21/09.
//  Copyright Embedthis Software 2009. All rights reserved.
//

#import "appweb_iphoneAppDelegate.h"

@implementation appweb_iphoneAppDelegate

@synthesize window;


- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    // Override point for customization after application launch
    [window makeKeyAndVisible];
}


- (void)dealloc {
    [window release];
    [super dealloc];
}


@end
