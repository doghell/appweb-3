//
//  appweb_iphoneAppDelegate.h
//  appweb-iphone
//
//  Created by Michael O'Brien on 5/21/09.
//  Copyright Embedthis Software 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface appweb_iphoneAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end

