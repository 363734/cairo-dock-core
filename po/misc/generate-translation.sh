#!/bin/sh
# Thanks to Jiro Kawada for his help !

if test "$1" = "all"; then
	export sources="../*/src/*.[ch] ../*/data/messages"  # plug-ins
else
	export sources="../src/*.[ch] ../src/*/*.[ch] ../data/messages"  # core
fi

#sed -i 's/\"More precisely/\/\* xgettext:no-c-format \*\/ \"More precisely/g' ../data/messages

xgettext -L C -k_ -k_D -kD_ -kN_ --no-wrap --from-code=UTF-8 --copyright-holder="Cairo-Dock project" --msgid-bugs-address="fabounet@glx-dock.org" -p . $sources -o cairo-dock.pot
#--omit-header

for lang in `ls *.po`
do
	echo -n "${lang} :"
	msgmerge ${lang} cairo-dock.pot -o ${lang}
	sed -i "/POT-Creation-Date/d" ${lang}
done;

sed -i "/POT-Creation-Date/d" cairo-dock.pot 

# For lp translation tool
if test -e 'en.po' -a -e 'en_GB.po'; then
	cp en.po en_GB.po
fi