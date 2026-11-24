horse 0x2d7 () {
	if (UcEvent == 0)  // Nearby...
	UI_item_say(UcItem, "Not too close, now.");
	else if (UcEvent == 1) UI_item_say(UcItem, "I am NOT Smith, foolish Avatar!");
}
