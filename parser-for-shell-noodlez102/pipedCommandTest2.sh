echo "ls | wc | wc -w | grep 3 > file2" | ./myshell

ans=$(ls | wc | wc -w | grep 3)

if [ -e "file2" ]; then
    # Check if the file only contains the correct output of the piped command
    if grep "$ans" "file2"; then
	    echo "PASS"
    else
	    echo "FAIL"
    fi
else
    echo "FAIL"
fi
