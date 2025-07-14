---
layout: default
title: Home
has_children: true
has_toc: false
permalink: /
nav_order: 1
---

# Manage and Protect your SPI Flash Chip!

Flashkeeper is a family of open-hardware add-ons for standard SOIC-8 and WSON-8 SPI NOR flash chips (as are commonly used for firmware storage in PCs and other electronics), providing functionality for write-protection control, SPI bus breakout, flash device emulation, reprogramming, and state validation.

![wponly]({{ site.baseurl }}/images/wponly_installed.png)
*The WP-only adapter provides a physical write protection switch for [flash chips supporting WP](https://github.com/linuxboot/flashkeeper/pull/14/files#diff-648b427619d21e01c18aefc3ff817e444286098d8048b6545a4089b1ead54682)*


<!-- markdownlint-disable MD033 -->
<details open markdown="block">
  <summary>
    Table of Contents
  </summary>
  {: .text-delta }
1. TOC
{:toc}
</details>
<!-- markdownlint-enable MD033 -->

## For Users
Flashkeeper is a modular family of several related devices - to find out which components you need, and where to get them, see [Getting Started](/Getting-Started).

## For Developers
Flashkeeper development takes place on the [15h.org](https://15h.org) Git forge, and is mirrored to Github as a backup. If you wish to participate in Flashkeeper development on 15h.org infrastructure, please request an account in either the [Flashkeeper room](https://matrix.to/#/#flashkeeper:matrix.org) or the 15h.org room on Matrix. Issues may be opened on either 15h.org or Github.

## More about Flashkeeper

* [Flashkeeper threat model]({{ site.baseurl }}/Flashkeeper-threat-model/) - goes into more detail about what classes of threats Flashkeeper attempts to counter.
* [Frequently Asked Questions]({{ site.baseurl }}/FAQ/)
* [Prior Art/State of the Art prior of project start](https://cfp.3mdeb.com/qubes-os-summit-2024/talk/FCENX9/)
