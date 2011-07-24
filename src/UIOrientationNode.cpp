//
//  BloomScene.cpp
//  Kepler
//
//  Created by Tom Carden on 7/17/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include "cinder/app/AppCocoaTouch.h" // for getElapsedSeconds, getWindowSize
#include "cinder/gl/gl.h"
#include "OrientationHelper.h"
#include "UIOrientationNode.h"
#include "BloomScene.h"

using namespace ci;
using namespace ci::app;

UIOrientationNode::UIOrientationNode( OrientationHelper *orientationHelper ): 
    mOrientationHelper(orientationHelper),
    mInterfaceAngle(0.0f),
    mTargetInterfaceSize(0.0f,0.0f),
    mTargetInterfaceAngle(0.0f),
    mLastOrientationChangeTime(-1.0f),
    mOrientationAnimationDuration(0.25f),
    mPrevInterfaceAngle(0.0f),
    mPrevInterfaceSize(0.0f,0.0f),
    mEnableAnimation(true),
    mCurrentlyAnimating(false)
{
    cbOrientationChanged = mOrientationHelper->registerOrientationChanged( this, &UIOrientationNode::orientationChanged );    
    // initialize orientation *without animating*
    setInterfaceOrientation( mOrientationHelper->getInterfaceOrientation(), false );
}

UIOrientationNode::~UIOrientationNode()
{
    mOrientationHelper->unregisterOrientationChanged( cbOrientationChanged );
}

bool UIOrientationNode::orientationChanged( OrientationEvent event )
{
    setInterfaceOrientation( event.getInterfaceOrientation(), mEnableAnimation );
    return false;
}

void UIOrientationNode::setInterfaceOrientation( const Orientation &orientation, bool animate )
{
    mInterfaceOrientation = orientation;
    
    const float TWO_PI = 2.0f * M_PI;
    
    // normalize current interface angle
    if (mInterfaceAngle < 0.0){
        float turns = floor( fabs( mInterfaceAngle / TWO_PI ) );
		mInterfaceAngle += turns * TWO_PI;
	}
    else if (mInterfaceAngle > TWO_PI){
        float turns = floor( mInterfaceAngle / TWO_PI );
		mInterfaceAngle -= turns * TWO_PI;
	}
    
    // get the facts
    Vec2f deviceSize = app::getWindowSize();
    float orientationAngle = getOrientationAngle( mInterfaceOrientation );
    
    // assign new targets
    mTargetInterfaceSize.x = fabs(deviceSize.x * cos(orientationAngle) + deviceSize.y * sin(orientationAngle));
    mTargetInterfaceSize.y = fabs(deviceSize.y * cos(orientationAngle) + deviceSize.x * sin(orientationAngle));
    mTargetInterfaceAngle = TWO_PI - orientationAngle;
    
    // make sure we're turning the right way
    if (abs(mTargetInterfaceAngle-mInterfaceAngle) > M_PI) {
        if (mTargetInterfaceAngle < mInterfaceAngle) {
            mTargetInterfaceAngle += TWO_PI;
        }
        else {
            mTargetInterfaceAngle -= TWO_PI;
        }
    }
    
    if (animate) {
        // remember current values for lerping later
        mPrevInterfaceAngle = mInterfaceAngle;
        mPrevInterfaceSize = mRoot->getInterfaceSize();
        // and reset the counter
        mLastOrientationChangeTime = app::getElapsedSeconds();
    }
    else {
        // just jump to the animation's end in next update
        mLastOrientationChangeTime = -1;                
    }

    // ensure update() does the right thing
    mCurrentlyAnimating = true;
}

void UIOrientationNode::update()
{
    // animate transition
    if (mCurrentlyAnimating) {
        float t = app::getElapsedSeconds() - mLastOrientationChangeTime;
        if (t < mOrientationAnimationDuration) {
            float p = t / mOrientationAnimationDuration;
            mRoot->setInterfaceSize( lerp(mPrevInterfaceSize, mTargetInterfaceSize, p) );
            mInterfaceAngle = lerp(mPrevInterfaceAngle, mTargetInterfaceAngle, p);
        }
        else {
            // ensure snap to final values
            mRoot->setInterfaceSize( mTargetInterfaceSize );
            mInterfaceAngle = mTargetInterfaceAngle;
            mCurrentlyAnimating = false;
        }
        
        // update matrix (for globalToLocal etc)
        mTransform.setToIdentity();
        mTransform.translate( Vec3f( app::getWindowCenter(), 0 ) );
        mTransform.rotate( Vec3f( 0, 0, mInterfaceAngle ) );
        mTransform.translate( Vec3f( mRoot->getInterfaceSize() * -0.5f, 0 ) );                
    }    
}

float UIOrientationNode::getOrientationAngle( const Orientation &orientation )
{
    switch (orientation) {
        case LANDSCAPE_LEFT_ORIENTATION:
            return M_PI * 1.5f;
        case UPSIDE_DOWN_PORTRAIT_ORIENTATION:
            return M_PI;
        case LANDSCAPE_RIGHT_ORIENTATION:
            return M_PI * 0.5f;
        case PORTRAIT_ORIENTATION:
        default:
            return 0.0;
    }
}