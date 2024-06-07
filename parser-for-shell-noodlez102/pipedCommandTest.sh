echo "ls /usr/local/bin | wc -l > file1" | ./myshell

ans=$(ls /usr/local/bin | wc -l)

if [ -e "file1" ]; then
    # Check if the file contains the correct output of the piped command
    if grep "$ans" "file1"; then
	    echo "PASS"
    else
	    echo "FAIL"
    fi
else
    echo "FAIL"
fi
