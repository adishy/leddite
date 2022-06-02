from setuptools import setup

with open("README.md", "r", encoding="utf-8") as fh:
    long_desc = fh.read()

# Generate requires

requires = [
  'docopt',
  'python-dotenv',
  #'imageio',
  'arrow==0.15.5',
  'bs4==0.0.1',
  'Flask',
  'requests==2.22.0',
  'sh==1.12.14',
  'rich',
 ]

requires_pi = [
  'rpi_ws281x'
 ]

# From: https://raspberrypi.stackexchange.com/a/118473
def is_raspberrypi():
    try:
        with io.open('/sys/firmware/devicetree/base/model', 'r') as m:
            if 'raspberry pi' in m.read().lower(): return True
    except Exception: pass
    return False

if is_raspberrypi():
    requires.append(requires_pi)

# Setup leddite package metadata
setup(
    name='leddite',
    version='0.1.11',
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
            'leddite=leddite:run_cli',
            'main=leddite:main'
        ],
    },
    install_requires=requires,
    python_requires=">=3.6",
)
