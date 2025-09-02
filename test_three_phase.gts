function x() {
    y();
    function y() {
        z();
    }
}

function z() {
    console.log("Hello from z!");
}

x();
