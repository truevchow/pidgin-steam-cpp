#!/bin/bash
# Read STEAM_USERNAME, STEAM_PASSWORD and STEAM_GUARD_CODE from stdin
echo -n "Username: "
read STEAM_USERNAME
echo -n "Password: "
read -s STEAM_PASSWORD
echo
echo -n "Steam Guard Code: "
read STEAM_GUARD_CODE

export STEAM_USERNAME="$STEAM_USERNAME"
export STEAM_PASSWORD="$STEAM_PASSWORD"
export STEAM_GUARD_CODE="$STEAM_GUARD_CODE"

echo "STEAM_USERNAME=$STEAM_USERNAME"
echo "STEAM_PASSWORD=$STEAM_PASSWORD"
echo "STEAM_GUARD_CODE=$STEAM_GUARD_CODE"