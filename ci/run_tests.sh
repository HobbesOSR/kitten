qemu-system-x86_64 -cdrom arch/x86_64/boot/image.iso -m 1024 -serial stdio -smp 4 -monitor /dev/null -nographic | tee /tmp/out &
sleep 20

if grep -q "FAILED" /tmp/out; then
    echo "test failed" 2>&1
    exit 1
fi

if grep -q "That's all, folks!" /tmp/out; then
    exit 0
fi
