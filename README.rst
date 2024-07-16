Zephyr Workshop
###############

Overview
********

This repository contains the code for Zephyr's Workshop used at Espressif Summit Brazil 2024.

Local set up
************

Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

Follow the instructions available in `Zephyr's Getting Started`_:

Then, proceed with the following steps:

.. code-block:: console

   cd ~
   mkdir zephyr-workshop
   python -m venv zephyr-workshop/.venv
   cd zephyr-workshop
   source .venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: console

   cd ~/zephyr-workshop
   west init -m https://github.com/sylvioalves/zephyr-workshop.git .
   west update
   west zephyr-export
   pip install -r zephyr/scripts/requirements.txt

Building the Workshop Applications
**********************************

Each numbered directory contains a different applications used in the workshop.

01_hello_world
======

.. code-block:: console

   # Enable the Python virtual environment
   $ cd ~/zephyr-workshop
   $ source .venv/bin/activate

   # Build the application
   $ (.venv) cd app
   # Choose one of the following build commands:
   # For esp-rust board, use esp_rs
   $ (.venv) west build -b esp_rs 01_hello_world

   # Flash the binary to your device
   $ (.venv) west flash

   # Open serial monitor
   $ (.venv) west espressif monitor

References
**********

.. target-notes::
    
.. _Zephyr's Getting Started:
   https://docs.zephyrproject.org/latest/develop/getting_started/index.html
