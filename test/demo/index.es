

public dynamic class _Solo_demo_indexView extends View {
    function _Solo_demo_indexView(c: Controller) {
        super(c)
    }

    override public function render() {

write("</html>
<html><head><title>index.html</title></head>
<body>");
require Acme

    var rocket = new Rocket()
    n = rocket.countdown()
    rocket.blastoff(view, n)

write("</body>
</html>
");

    }
}
