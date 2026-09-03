#!/bin/bash

echo -e "\n========== BLUEGACHIIN ==========\n"

if [[ "$1" == "-e" ]]; then

	read -p "Enter target image path: " TARGET_IMG_PATH
	read -p "Enter destination directory: " DEST_DIR
	read -p "Enter payload: " PAYLOAD

	cp "$TARGET_IMG_PATH" .
	IMG_TITLE=$(basename "$TARGET_IMG_PATH")
	IMG_EXT=${IMG_TITLE##*.}

	make

	if [[ "$IMG_EXT" == "png" ]]; then
		./bin/bg --encode --png "$IMG_TITLE" "$PAYLOAD" "$BG_SECRET_KEY"
	elif [[ "$IMG_EXT" == "jpg" ]]; then
		./bin/bg --encode --jpg "$IMG_TITLE" "$PAYLOAD" "$BG_SECRET_KEY"
	fi

	mv "F_*.png" "$DEST_DIR"
	rm "$IMG_TITLE"

elif [[ "$1" == "-d" ]]; then

	read -p "Enter path of image to decode: " TARGET_IMG_PATH

	cp "$TARGET_IMG_PATH" .
	IMG_TITLE=$(basename "$TARGET_IMG_PATH")
	IMG_EXT=${IMG_TITLE##*.}

	make
	if [[ "$IMG_EXT" == "png" ]]; then
		./bin/bg --decode --png "$IMG_TITLE" "$BG_SECRET_KEY"
	elif [[ "$IMG_EXT" == "jpg" ]]; then
		./bin/bg --decode --jpg "$IMG_TITLE" "$BG_SECRET_KEY"
	fi

	rm "$IMG_TITLE"

fi
