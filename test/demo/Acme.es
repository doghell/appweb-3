module Acme {

    require ejs.web

    class Rocket {
        function Rocket() {
            print("Creating a new rocket")
        }

        public native function countdown()

        public function blastoff(v: View, delay) {
            breakpoint()
            v.write("blastoff in " + delay + " seconds")
        }
    }
}
