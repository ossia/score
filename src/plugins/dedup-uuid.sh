

egrep -roh '[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}' | sort | uniq -c | grep -v ' 1 ' | awk '{ print $2 ; }' | xargs -I{} grep -R {} . 

