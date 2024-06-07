echo "touch file.txt" | ./myshell

if [ -e "file.txt" ]; then
    echo "PASS"
else
    echo "FAIL"
fi
