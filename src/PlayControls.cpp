/*
 *  PlayControls.h
 *  Kepler
 *
 *  Created by Tom Carden on 3/7/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "PlayControls.h"
#include "Globals.h"
#include "BloomGl.h"

void PlayControls::setup( AppCocoaTouch *app, bool initialPlayState )
{
    mApp			= app;
    // TODO: unregister these in destructor!
    cbTouchesBegan	= mApp->registerTouchesBegan( this, &PlayControls::touchesBegan );
    cbOrientationChanged = mApp->registerOrientationChanged( this, &PlayControls::orientationChanged );    
    cbTouchesEnded	= 0;
    cbTouchesMoved	= 0;		
    lastTouchedType = NO_BUTTON;
    mIsPlaying		= initialPlayState;
    mMinutes		= 0;
    mSeconds		= 60;
    mPrevSeconds	= 0;
    mIsDraggingPlayhead = false; 
    setInterfaceOrientation( app->getInterfaceOrientation() );
	
	mHelpPer = 0.0f;
}

void PlayControls::initHelpTextures( const Font &font )
{
	vector<string> names;
	names.push_back( "• FLY TO CURRENT TRACK" );
	names.push_back( "• CURRENT TRACK INFO" );
	names.push_back( "SHOW/HIDE NAV •" );
	names.push_back( "• SHOW/HIDE HELP" );
	names.push_back( "• SHOW/HIDE ORBIT LINES" );
	names.push_back( "• SHOW/HIDE TEXT" );
	
	int count = 0;
	for( vector<string>::iterator it = names.begin(); it != names.end(); ++it ){
		TextLayout layout;	
		layout.setFont( font );
		layout.setColor( ColorA( COLOR_BRIGHT_YELLOW, 1.0f ) );
		layout.addCenteredLine( names[count] );
		mHelpTextures.push_back( gl::Texture( layout.render( true, false ) ) );
		
		count ++;
	}
}

void PlayControls::update()
{
    // TODO: update anything time based here, e.g. elapsed time of track playing
    // or e.g. button animation
    
    // clean up listeners here, because if we remove in touchesEnded then things get crazy
    if (mApp->getActiveTouches().size() == 0 && cbTouchesEnded != 0) {
        mApp->unregisterTouchesEnded( cbTouchesEnded );
        mApp->unregisterTouchesMoved( cbTouchesMoved );
        cbTouchesEnded = 0;
        cbTouchesMoved = 0;
    }		
}

bool PlayControls::touchesBegan( TouchEvent event )
{
    Rectf transformedBounds = transformRect( lastDrawnBoundsRect, mOrientationMtx );
    vector<TouchEvent::Touch> touches = event.getTouches();
    if (touches.size() > 0 && transformedBounds.contains(touches[0].getPos())) {
        if (cbTouchesEnded == 0) {
            cbTouchesEnded = mApp->registerTouchesEnded( this, &PlayControls::touchesEnded );
            cbTouchesMoved = mApp->registerTouchesMoved( this, &PlayControls::touchesMoved );			
        }

        lastTouchedType = findButtonUnderTouches(touches);
		
        if( lastTouchedType == SLIDER ){
            mIsDraggingPlayhead = true;
        }
        
        return true;
    }
    else {
        lastTouchedType = NO_BUTTON;
    }
    return false;
}

bool PlayControls::touchesMoved( TouchEvent event )
{
    vector<TouchEvent::Touch> touches = event.getTouches();

    lastTouchedType = findButtonUnderTouches(touches);

    if( mIsDraggingPlayhead ){
        if( touches.size() == 1 ){
            
            Vec2f pos = touches.begin()->getPos();
            pos = (mOrientationMtx.inverted() * Vec3f(pos,0)).xy();
            
			// MAGIC NUMBER: beware these hard-coded nasties!
            float border = 110.0f;
			float sliderLength = mInterfaceSize.x * 0.3f;
            float playheadPer = ( pos.x - border ) / sliderLength;
            
            // TODO: Make this callback take a double (between 0 and 1?)
            mCallbacksPlayheadMoved.call( constrain( playheadPer, 0.0f, 1.0f ) );
        }
    }
    return false;
}	

bool PlayControls::touchesEnded( TouchEvent event )
{
    vector<TouchEvent::Touch> touches = event.getTouches();
    if( lastTouchedType != NO_BUTTON && lastTouchedType == findButtonUnderTouches(touches) ){
        mCallbacksButtonPressed.call(lastTouchedType);
    }

    lastTouchedType = NO_BUTTON;
    mIsDraggingPlayhead = false;
    return false;
}

bool PlayControls::orientationChanged( OrientationEvent event )
{
    if (event.getInterfaceOrientation() != mInterfaceOrientation) {
        setInterfaceOrientation( event.getInterfaceOrientation() );
    }
    return false;
}

void PlayControls::setInterfaceOrientation( const Orientation &orientation )
{
    mInterfaceOrientation = orientation;
    
    Vec2f deviceSize = app::getWindowSize();
    
    mOrientationMtx = getOrientationMatrix44<float>(orientation);
    
    mInterfaceSize = deviceSize;
    
    if ( isLandscapeOrientation(orientation) ) {
        mInterfaceSize = mInterfaceSize.yx();
    }
	
	mHelpLocs[0] = Vec2f( 35.0f, mInterfaceSize.y - 110.0f );
	mHelpLocs[1] = Vec2f( 70.0f, mInterfaceSize.y - 85.0f );
	mHelpLocs[2] = Vec2f( mInterfaceSize.x * 0.5f - 95.0f, mInterfaceSize.y - 85.0f );
	mHelpLocs[3] = Vec2f( mInterfaceSize.x - 292.0f, mInterfaceSize.y - 163.0f );
	mHelpLocs[4] = Vec2f( mInterfaceSize.x - 257.0f, mInterfaceSize.y - 137.0f );
	mHelpLocs[5] = Vec2f( mInterfaceSize.x - 222.0f, mInterfaceSize.y - 110.0f );
}

void PlayControls::draw( const Orientation &orientation, const gl::Texture &uiButtonsTex, const gl::Texture &currentTrackTex, AlphaWheel *alphaWheel, const Font &font, float y, float currentTime, float totalTime, float secsSinceTrackChange )
{   
	float dragAlphaPer = pow( ( mInterfaceSize.y - y ) / 65.0f, 2.0f );
	
    gl::pushModelView();
    gl::multModelView( mOrientationMtx );    
    
    lastDrawnBoundsRect = Rectf(0, y, mInterfaceSize.x, mInterfaceSize.y );
    
    //gl::color( ColorA( 0.0f, 0.0f, 0.0f, 0.0f ) );
    //gl::drawSolidRect( lastDrawnBoundsRect ); // TODO: make height settable in setup()?
    
    touchRects.clear();
    touchTypes.clear(); // technically touch types never changes, but whatever
    
    float bSize			= 50.0f;
    float bSizeSmall	= 40.0f;
	
	float timeTexWidth	= 55.0f;
	float topBorder		= 7.0f;
	float sideBorder	= 10.0f;
	float buttonGap		= 1.0f;
	float sliderWidth	= 201.0f;
	if ( isLandscapeOrientation(orientation) ) {
        sliderWidth = 328.0f;
    }
    float sliderHeight	= 20.0f;
    float sliderInset	= bSize + timeTexWidth;
	float x1, x2, y1, y2;
	
	
	y1 = y + topBorder;
	y2 = y1 + bSize;
	
	
	x1 = sideBorder;
	x2 = x1 + bSize;
	Rectf currentTrackButton( x1, y1, x2, y2 );
	
	x1 = mInterfaceSize.x / 2 - bSize/2;//bSize + timeTexWidth * 2.0f + sliderWidth;
	x2 = x1 + bSize;
	Rectf showWheelButton( x1, y1, x2, y2 );
	
	x1 = mInterfaceSize.x - sideBorder - bSize;
	x2 = x1 + bSize;
	Rectf nextButton( x1, y1, x2, y2 );
	
	x1 -= bSize + buttonGap;
	x2 = x1 + bSize;
	Rectf playButton( x1, y1, x2, y2 );
	
	x1 -= bSize + buttonGap;
	x2 = x1 + bSize;
	Rectf prevButton( x1, y1, x2, y2 );

	
	y1 += 5.0f;
	x1 -= bSize * 1.625f;
	x2 = x1 + bSizeSmall;
	Rectf drawTextButton( x1, y1, x2, y1 + bSizeSmall );
	
	x1 -= bSizeSmall - 5.0f;
	x2 = x1 + bSizeSmall;
	Rectf drawRingsButton( x1, y1, x2, y1 + bSizeSmall );
	
	x1 -= bSizeSmall - 5.0f;
	x2 = x1 + bSizeSmall;
	Rectf helpButton( x1, y1, x2, y1 + bSizeSmall );
	
    
    float playheadPer	= 0.0f;
    if( totalTime > 0.0f ){
        playheadPer = constrain<float>(currentTime/totalTime, 0.0, 1.0);
    }
    float bgx1			= sliderInset;
    float bgx2			= bgx1 + sliderWidth;
    float bgy1			= y + 32.0f;
    float bgy2			= bgy1 + sliderHeight;
    float fgx1			= sliderInset;
    float fgx2			= fgx1 + ( sliderWidth * playheadPer );
    float fgy1			= bgy1;
    float fgy2			= bgy2;
    
    Rectf sliderBg( bgx1, bgy1, bgx2, bgy2 );
    Rectf sliderFg( fgx1, fgy1, fgx2, fgy2 );
    Rectf sliderButton( fgx2 - 14.0f, ( fgy1 + fgy2 ) * 0.5f - 14.0f, fgx2 + 14.0f, ( fgy1 + fgy2 ) * 0.5f + 14.0f );
	
	
	Rectf currentTrackRect( 0.0f, 0.0f, 0.0f, 0.0f );
	float ctx1, ctx2, cty1, cty2, ctw;
	if( currentTrackTex ){
		ctx1 = bgx1 - 43.0f;
		ctx2 = bgx2 + 50.0f;
		cty1 = bgy1 - 14.0f;
		cty2 = cty1 + currentTrackTex.getHeight();
		ctw = ctx2 - ctx1;
		currentTrackRect = Rectf( ctx1, cty1, ctx2, cty2 );
	}
	
	touchRects.push_back( currentTrackButton );
	touchTypes.push_back( CURRENT_TRACK );
	touchRects.push_back( showWheelButton );
	touchTypes.push_back( SHOW_WHEEL );
    touchRects.push_back( prevButton );
    touchTypes.push_back( PREVIOUS_TRACK );
    touchRects.push_back( playButton );
    touchTypes.push_back( PLAY_PAUSE );
    touchRects.push_back( nextButton );
    touchTypes.push_back( NEXT_TRACK );
    touchRects.push_back( sliderButton );
    touchTypes.push_back( SLIDER );
    touchRects.push_back( helpButton );
    touchTypes.push_back( HELP );
    touchRects.push_back( drawRingsButton );
    touchTypes.push_back( DRAW_RINGS );
    touchRects.push_back( drawTextButton );
    touchTypes.push_back( DRAW_TEXT );
	
	gl::color( ColorA( 1.0f, 1.0f, 1.0f, dragAlphaPer ) );
    uiButtonsTex.enableAndBind();
	gl::enableAlphaBlending();
	
	
	float u1, u2, v1, v2, v3;
	v1 = 0.0f; v2 = 0.25f; v3 = 0.5f;
	float uw = 1.0f/8.0f;
// CURRENT TRACK
	u1 = uw * 0.0f;
	u2 = u1 + uw;
    if( lastTouchedType == CURRENT_TRACK )
		drawButton( currentTrackButton, u1, v2, u2, v3 );
    else
		drawButton( currentTrackButton, u1, v1, u2, v2 );

	
// SHOW WHEEL
	u1 = uw * 1.0f;
	u2 = u1 + uw;
    if( alphaWheel->getShowWheel() )
		drawButton( showWheelButton, u1, v2, u2, v3 );
    else
		drawButton( showWheelButton, u1, v1, u2, v2 );	
	
	
// PREV
	u1 = uw * 2.0f;
	u2 = u1 + uw;
    if( lastTouchedType == PREVIOUS_TRACK )
		drawButton( prevButton, u1, v2, u2, v3 );
    else
		drawButton( prevButton, u1, v1, u2, v2 );
    
    
// PLAY/PAUSE	
	if( ! mIsPlaying )
		u1 = uw * 3.0f;
	else
		u1 = uw * 4.0f;
	u2 = u1 + uw;
	if( lastTouchedType == PLAY_PAUSE )
		drawButton( playButton, u1, v2, u2, v3 );
	else
		drawButton( playButton, u1, v1, u2, v2 );
    
// NEXT
	u1 = uw * 5.0f;
	u2 = u1 + uw;
    if( lastTouchedType == NEXT_TRACK )
		drawButton( nextButton, u1, v2, u2, v3 );
    else
		drawButton( nextButton, u1, v1, u2, v2 );
	
	
	uw = 1.0f/10.0f;
	
	
// HELP
	u1 = 0.0f; u2 = u1 + uw;
	v1 = 0.5f; v2 = 0.7f; v3 = 0.9f;
	if( G_HELP )
		drawButton( helpButton, u1, v2, u2, v3 );
	else 
		drawButton( helpButton, u1, v1, u2, v2 );		
	
// DRAW RINGS
	u1 = uw * 1.0f; u2 = u1 + uw;
    if( G_DRAW_RINGS )
		drawButton( drawRingsButton, u1, v2, u2, v3 );
	else 
		drawButton( drawRingsButton, u1, v1, u2, v2 );
	
// DRAW TEXT
	u1 = uw * 2.0f; u2 = u1 + uw;
	if( G_DRAW_TEXT )
		drawButton( drawTextButton, u1, v2, u2, v3 );
	else 
		drawButton( drawTextButton, u1, v1, u2, v2 );
	
	
// SLIDER BG
	u1 = 0.0f; u2 = uw;
	v1 = 0.9f; v2 = 1.0f;
    drawButton( sliderBg, u1, v1, u2, v2 );
	
	gl::enableAdditiveBlending();
	
// SLIDER FG
	u1 = uw * 2.0; u2 = uw * 3.0f;
    drawButton( sliderFg, u1, v1, u2, v2 );
	
	gl::enableAlphaBlending();
	
// SLIDER BUTTON
	v1 = 0.5f; v2 = 0.7f; v3 = 0.9f;
	u1 = uw * 3.0f; u2 = u1 + uw;
    if( lastTouchedType == SLIDER )
		drawButton( sliderButton, u1, v2, u2, v3 );
	else 
		drawButton( sliderButton, u1, v1, u2, v2 );
    

// CURRENT TRACK
	if( currentTrackTex ){
		currentTrackTex.enableAndBind();
		float ctSpaceWidth = ctw;
		float ctTexWidth = currentTrackTex.getWidth();
		float ctu1 = 0.0f;
		float ctu2;
		float ctPer = ctSpaceWidth/ctTexWidth;
		
		if( ctTexWidth < ctSpaceWidth ){ // if the texture width is less than the rect to fit it in...
			ctu2 = ctPer;
			drawButton( currentTrackRect, ctu1, 0.0f, ctu2, 1.0f );
			
		} else {				// if the texture is too wide, animate the u coords
			
			float x1;
			if( app::getElapsedSeconds() - secsSinceTrackChange > 5.0f ){	// but wait 5 seconds first
				x1 = fmodf( ( app::getElapsedSeconds() - ( secsSinceTrackChange + 5.0f ) )*20.0f, ctTexWidth + 10.0f ) - ctTexWidth-10.0f;
			} else {
				x1 = -ctTexWidth-10.0f;
			}
			
			Area a1 = Area( x1, 0.0f, x1 + ctSpaceWidth, currentTrackTex.getHeight() );
			gl::draw( currentTrackTex, a1, currentTrackRect );
			
			Area a2 = Area( x1 + ctTexWidth + 10.0f, 0.0f, x1 + ctTexWidth + 10.0f + ctSpaceWidth, currentTrackTex.getHeight() );
			gl::draw( currentTrackTex, a2, currentTrackRect );			

			Area aLeft		 = Area( 200.0f, 140.0f, 214.0f, 150.0f ); 
			Rectf coverLeft  = Rectf( currentTrackRect.x1, currentTrackRect.y1, currentTrackRect.x1 + 10.0f, currentTrackRect.y2 );
			Rectf coverRight = Rectf( currentTrackRect.x2 + 1.0f, currentTrackRect.y1, currentTrackRect.x2 - 9.0f, currentTrackRect.y2 );
			
			gl::draw( uiButtonsTex, aLeft, coverLeft );
			gl::draw( uiButtonsTex, aLeft, coverRight );
		}
	}
	
	
	
	
	
    // CURRENT TIME
    mMinutes	= floor( abs(currentTime)/60.0f );
    mPrevSeconds = mSeconds;
    mSeconds	= (int)abs(currentTime)%60;
    
    mMinutesTotal	= floor( totalTime/60.0f );
    mSecondsTotal	= (int)totalTime%60;
    
    double timeLeft = min(totalTime, totalTime - currentTime);
    mMinutesLeft	= floor( timeLeft/60.0f );
    mSecondsLeft	= (int)timeLeft%60;
    
    if( mSeconds != mPrevSeconds ){
        string minsStr = to_string( abs(mMinutes) );
        string secsStr = to_string( abs(mSeconds) );
        if( minsStr.length() == 1 ) minsStr = "0" + minsStr;
        if( secsStr.length() == 1 ) secsStr = "0" + secsStr;		
        
        stringstream ss;
        ss << minsStr << ":" << secsStr << endl;
        
        TextLayout layout;
        layout.setFont( font );
        layout.setColor( COLOR_BRIGHT_BLUE );
        layout.addLine( ss.str() );
        mCurrentTimeTex = layout.render( true, false );
        
        
        
        minsStr = to_string( mMinutesLeft );
        secsStr = to_string( mSecondsLeft );
        if( minsStr.length() == 1 ) minsStr = "0" + minsStr;
        if( secsStr.length() == 1 ) secsStr = "0" + secsStr;		
        
        ss.str("");
        ss << "-" << minsStr << ":" << secsStr;
        TextLayout layout2;
        layout2.setFont( font );
        layout2.setColor( COLOR_BRIGHT_BLUE );
        layout2.addLine( ss.str() );
        mRemainingTimeTex = layout2.render( true, false );
    }
    if (currentTime < 0) {
        TextLayout layout3;
        layout3.setFont( font );
        layout3.setColor( COLOR_BRIGHT_BLUE );
        layout3.addLine( "-" );
        gl::Texture hyphenTex = layout3.render( true, false );        
        gl::draw( hyphenTex,   Vec2f( bgx1 - 40.0f - hyphenTex.getWidth(), bgy1 + 2 ) );
    }
    gl::draw( mCurrentTimeTex,   Vec2f( bgx1 - 40.0f, bgy1 + 2 ) );
    gl::draw( mRemainingTimeTex, Vec2f( bgx2 + 8.0f, bgy1 + 2 ) );
	
	
	
	if( G_HELP ){
		mHelpPer -= ( mHelpPer - 1.0f ) * 0.2f;
	} else {
		mHelpPer -= mHelpPer * 0.2f;
	}
	
	drawHelp( y, dragAlphaPer, isLandscapeOrientation(orientation) );
    
    gl::popModelView();
	
	//    gl::color( Color( 1.0f, 0, 0 ) );
	//    gl::drawSolidRect( transformRect( lastDrawnBoundsRect, mOrientationMtx ) );
}

void PlayControls::drawHelp( float y, float dragAlphaPer, bool isLandscape )
{		
	if( mHelpPer > 0.001f ){
		float yOffset = 65.0f - ( mInterfaceSize.y - y );
		gl::color( ColorA( Color::white(), mHelpPer * dragAlphaPer ) );
		
		int count = 0;
		for( vector<gl::Texture>::iterator it = mHelpTextures.begin(); it != mHelpTextures.end(); ++it ){
			Vec2f offset( -3.0f, -15.0f + yOffset  );
			gl::draw( *it, mHelpLocs[count] + offset );
			count ++;
		}
		
		glDisable( GL_TEXTURE_2D );
		gl::color( ColorA( COLOR_BRIGHT_YELLOW, 0.25f * mHelpPer * dragAlphaPer ) );
		for( int i=0; i<6; i++ ){
			Vec2f offset = Vec2f( 0.0f, yOffset );
			
			if( isLandscape ){
				if( i == 2 ){
					offset.x = 95.0f;
				} else {
					offset.x -= 1.0f;
				}
			} else {
				if( i == 2 ){
					offset.x = 96.0f;
				}
			}
			
			gl::drawLine( mHelpLocs[i] + offset, Vec2f( mHelpLocs[i].x, mInterfaceSize.y - 66.0f ) + offset );		
		}
	}
}


PlayControls::PlayButton PlayControls::findButtonUnderTouches(vector<TouchEvent::Touch> touches) {
    Rectf transformedBounds = transformRect( lastDrawnBoundsRect, mOrientationMtx );
    for (int j = 0; j < touches.size(); j++) {
        TouchEvent::Touch touch = touches[j];
        Vec2f pos = touch.getPos();
        if ( transformedBounds.contains( pos ) ) {
            for (int i = 0; i < touchRects.size(); i++) {
                Rectf rect = transformRect( touchRects[i], mOrientationMtx );
//                if (touchTypes[i] == SLIDER) {
//                    std::cout << "testing slider rect: " << touchRects[i] << std::endl;
//                    std::cout << "      transformRect: " << rect << std::endl;
//                    std::cout << "                pos: " << pos << std::endl;
//                }
                if (rect.contains(pos)) {
//                    if (touchTypes[i] == SLIDER) {
//                        std::cout << "HIT slider rect!" << std::endl;
//                    }
                    return touchTypes[i];
                }
            }		
        }
    }		
    return NO_BUTTON;
}

// TODO: move this to an operator in Cinder's Matrix class?
Rectf PlayControls::transformRect( const Rectf &rect, const Matrix44f &matrix )
{
    Vec2f topLeft = (matrix * Vec3f(rect.x1,rect.y1,0)).xy();
    Vec2f bottomRight = (matrix * Vec3f(rect.x2,rect.y2,0)).xy();
    Rectf newRect(topLeft, bottomRight);
    newRect.canonicalize();    
    return newRect;
}
