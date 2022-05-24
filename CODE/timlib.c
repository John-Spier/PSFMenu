GsSPRITE PrepSprite (GsIMAGE TimImage);
GsIMAGE LoadTIM (u_long *tMemAddress);

GsSPRITE PrepSprite (GsIMAGE TimImage) {

	// Returns a GsSPRITE structure so that TimImage can be displayed using GsSortSprite.

	GsSPRITE tSprite;
		
	// Set texture size and coordinates
	switch (TimImage.pmode & 3) {
		case 0: // 4-bit
			tSprite.w = TimImage.pw << 2;
			tSprite.u = (TimImage.px & 0x3f) * 4;
			break;
		case 1: // 8-bit
			tSprite.w = TimImage.pw << 1;
			tSprite.u = (TimImage.px & 0x3f) * 2;
			break;
		default: // 16-bit
			tSprite.w = TimImage.pw;
			tSprite.u = TimImage.px & 0x3f; 
    };
	
	tSprite.h = TimImage.ph;
	tSprite.v = TimImage.py & 0xff;

	// Set texture page and attributes
	tSprite.tpage 		= GetTPage((TimImage.pmode & 3), 0, TimImage.px, TimImage.py);
	tSprite.attribute 	= (TimImage.pmode & 3) << 24;
	
	// CLUT coords
	tSprite.cx 			= TimImage.cx;
	tSprite.cy 			= TimImage.cy;
	
	// Default position, color intensity, and scale/rotation values
	tSprite.x = tSprite.y 				= 0;
	tSprite.mx = tSprite.my				= 0;
	tSprite.r = tSprite.g = tSprite.b 	= 128;
	tSprite.scalex = tSprite.scaley 	= ONE;
	tSprite.rotate 						= 0;
	
	return tSprite;

}

GsIMAGE LoadTIM (u_long *tMemAddress) {

	RECT tRect;
	GsIMAGE tTim;

	DrawSync(0);
	tMemAddress++;
	GsGetTimInfo(tMemAddress, &tTim);	// SAVE TIM-Info in TIM

	tRect.x = tTim.px;
	tRect.y = tTim.py;
	tRect.w = tTim.pw;
	tRect.h = tTim.ph;
	LoadImage(&tRect, tTim.pixel);		// Load TIM-DATA into VideoRam
	DrawSync(0);

	if ((tTim.pmode >> 3) & 0x01)
	{

		tRect.x = tTim.cx;
		tRect.y = tTim.cy;
		tRect.w = tTim.cw;
		tRect.h = tTim.ch;
		LoadImage(&tRect, tTim.clut);	// load CLUT into VideoRam

	}

	DrawSync(0);				// wait until GPU is ready
	return tTim;

}