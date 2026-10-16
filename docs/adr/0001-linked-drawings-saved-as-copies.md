# Linked Drawings are saved as full copies

A linked Drawing shares its pixels with the Drawings it is linked to, but each one is still saved as its own image, with an optional `linkedTo` attribute that names the image it follows. Project files must stay readable by older JustAnimate and Pencil2D, and a Drawing that only pointed at another image would open empty there; a full copy opens as an independent Drawing, so nothing is lost. The cost is file size, paid on purpose: do not "optimise" the copies away.
