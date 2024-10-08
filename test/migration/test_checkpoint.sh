#!/bin/bash
realpath() {
  case "$1" in /*) ;; *) printf '%s/' "$PWD";; esac; echo "$1"
}
wasmedge=$(realpath ../../build/tools/wasmedge/wasmedge)


test-checkpoint() {
    dir=$1
    app=$2

    pushd $dir

    # 1000回dispatchしたところでcheckpointする
    $wasmedge \
        --image-dir actual-images \
        --dispatch-limit 1000 \
        $app

    # 実行が失敗すればfailed
    if [ $? != 0 ]; then
        echo "execution failure"
        rm ./actual-images/*
        exit 1
    fi

    # 出力されたimagesがexpect-imagesと同じ確認する
    diff=$(diff ./expect-images ./actual-images)
    status=$?
    if [ $status -eq 0 ]; then
        echo "Success" 
    else
        echo "Failed"
        diff ./expect-images ./actual-images

        rm ./actual-images/*
        exit 1
    fi

    popd
}


# dir="apps/binary-trees"
# app="binary-trees.wasm 13"
test-checkpoint "apps/binary-trees" "binary-trees.wasm"
test-checkpoint "apps/n-body"       "n-body.wasm"

echo OK