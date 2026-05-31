---
layout: default
title: Cables and Connectors
permalink: /Cables-Connectors/
nav_order: 99
parent: Development
---

Cables and Connectors
{: .fs-8 .m-0 }

![design]({{ site.baseurl }}/images/ffc.jpg)

Flashkeeper has standardized on two common types of flat-flex cable (FFC) - one for Flashkeeper FPGA hardware, one for breakout chip adapters. All FFCs used by Flashkeeper are standard 0.5mm pitch B-type (opposite-side-contact) cables, as are commonly used in laptops and other consumer electronics. Lengths between 1cm and 50cm are widely and inexpensively available online from websites such as [AliExpress](https://www.aliexpress.com/w/wholesale-0.5mm-b%2525252dtype-ffc.html?spm=a2g0o.productlist.search.0), [eBay](https://www.ebay.com/sch/i.html?_nkw=0.5mm+b+type+ffc), and [Amazon](https://www.amazon.com/s?k=0.5mm+b+type+ffc), and are generally interchangeable between brands. If purchasing FFCs for Flashkeeper hardware, for best signal integrity, opt for the shortest cable which will reach in your chassis.

While both are common sizes, note that cables are NOT interchangeable between Flashkeeper breakout chip adapters (including dummy boards), and Flashkeeper FPGA hardware - these devices carry different signals over different pinouts, and the FPGA hardware requires additional pins for use with the FPGA Configuration Tool.

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

---
### Breakout Chip Adapters
Flashkeeper FPGA hardware uses standard **8-pin** 0.5mm FFC, and has standardized on the TE Connectivity 2328702-8 flip-lock connector. If you wish to design flash device breakout hardware to be interoperable with Flashkeeper breakout hardware, the pinout is provided below - note that "pin 11" refers to the front grounding tabs of the connector, and does not appear on the cable itself.

![design]({{ site.baseurl }}/images/pinout_breakout.png)


### FPGA Hardware
Flashkeeper FPGA hardware uses standard **10-pin** 0.5mm FFC, and has standardized on the TE Connectivity 1-2328702-0 flip-lock connector. If you wish to design sensors, adapters, or other peripherals to be interoperable with Flashkeeper FPGA hardware, the pinout is provided below - note that "pin 9" refers to the front grounding tabs of the connector, and does not appear on the cable itself.

![design]({{ site.baseurl }}/images/pinout_fpga.png)
