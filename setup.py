from setuptools import setup

with open("README.md", "r", encoding="utf-8") as fh:
    long_desc = fh.read()
setup(
    name='leddite',
    version='0.1.9',
    author="Aditya Shylesh",
    author_email="development-mails@adishy.com",
    description="leddite lets you create your own art with Neopixel displays easily",
    long_description=long_desc,
    long_description_content_type="text/markdown",
    url="https://github.com/adishy/leddite",
    project_url = {
        "Bug Tracker": "https://github.com/adishy/leddite.issues"
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    packages=['leddite'],
    include_package_data=True,
    entry_points={
        'console_scripts': [
            'main=leddite:main'
        ],
    },
    install_requires=[
        'docopt',
        'python-dotenv',
        'imageio',
        'arrow==0.15.5',
        'bs4==0.0.1',
        'Flask',
        'requests==2.22.0',
        'sh==1.12.14',
        'rich',
        "rpi_ws281x;platform_system=='Linux'"
    ],
    python_requires=">=3.6",
)
