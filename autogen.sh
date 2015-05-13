#!/bin/sh

echo -n "checking for git... "
[ -d .git ] && which git || {
	echo "*** No git found, please install it ***"
	exit 1
}

echo "running git submodule init"
git submodule init || exit $?

echo "running git submodule update"
git submodule update || exit $?

echo -n "checking for pkg-config... "
which pkg-config || {
	echo "*** No pkg-config found, please install it ***"
	exit 1
}

echo -n "checking for autoreconf... "
which autoreconf || {
	echo "*** No autoreconf (autoconf) found, please install it ***"
	exit 1
}

echo "running autoreconf --force --install --verbose"
autoreconf --force --install --verbose || exit $?
