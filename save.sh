#!/bin/bash

save_info=$@

if [ -z "${save_info}" ]; then
	save_info="save"
fi

echo "commit message is  ${save_info}"

git commit -am "${save_info}"

git push
