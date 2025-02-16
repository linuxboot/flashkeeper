---
layout: default
title: About
has_children: true
has_toc: false
permalink: /
nav_order: 1
---

<!-- markdownlint-disable MD033 -->
<details open markdown="block">
  <summary>
    Table of contents
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>
<!-- markdownlint-enable MD033 -->

# Overview

Flashkeeper is a device designed for installation inside a computer, connecting to the SPI flash chip where firmware is stored and making write protection control and reprogramming easy and secure.

![wponly]({{ site.baseurl }}/images/wponly.png)
A WP-only soldered adapter, to be soldered onto the flash chip from above and offering a write protection switch for [SPI chips supporting WP](https://github.com/linuxboot/flashkeeper/pull/14/files#diff-648b427619d21e01c18aefc3ff817e444286098d8048b6545a4089b1ead54682) (KiCAD)

![pogopins]({{ site.baseurl }}/images/chip.png)
Spring-loaded contacts (pogo pins) interfacing with a SOIC-8 flash chip from above, as in the solderless adapter (FreeCAD)

## Further reading

* [Flashkeeper - Original project scope - Presentation at QubesOS mini-summit 2024](https://cfp.3mdeb.com/qubes-os-summit-2024/talk/FCENX9/)

## Learn more about Flashkeeper

* [Flashkeeper threat model]({{ site.baseurl }}/Flashkeeper-threat-model/) - goes into more detail about what classes of threats Flashkeeper attempts to counter.
* [Frequently Asked Questions]({{ site.baseurl }}/FAQ/)
* [Prior Art/State of the Art prior of project start](https://cfp.3mdeb.com/qubes-os-summit-2024/talk/FCENX9/)
