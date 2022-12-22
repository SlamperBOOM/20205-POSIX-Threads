import aiohttp
import asyncio
import time

PROXY = "http://127.0.0.1:8081"
HOST = "http://ccfit.nsu.ru/~rzheutskiy/test_files/"

SMALL = "50mb.dat"
MEDIUM = "100mb.dat"


async def do_request(additional_path):
    path = f"{HOST}{additional_path}"
    print(f"Do request to {path}.")
    headers = {"Connection": "close"}
    ts = time.time()
    async with aiohttp.ClientSession() as session:
        async with session.get(path, headers=headers, proxy=PROXY) as resp:
            print(f"Read size: {len(await resp.read())}")
            print("Status:", resp.status)
    te = time.time()
    print(f"Time: {te - ts} sec.")
    print()


async def main():

    print("Send one request for small file")
    await do_request("50mb.dat")

    print("Send 5 async requests for small file")
    await asyncio.gather(
        do_request("50mb.dat"),
        do_request("50mb.dat"),
        do_request("50mb.dat"),
        do_request("50mb.dat"),
        do_request("50mb.dat"),
    )

    print("Send one request for medium file")
    await do_request("100mb.dat")

    print("Send 5 async requests for medium file")
    await asyncio.gather(
        do_request("100mb.dat"),
        do_request("100mb.dat"),
        do_request("100mb.dat"),
        do_request("100mb.dat"),
        do_request("100mb.dat"),
    )


if __name__ == '__main__':
    asyncio.run(main())
