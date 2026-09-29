import re
import os

file_path = "HelpSource/Guides/CamaraObscura.schelp"

with open(file_path, 'r') as f:
    content = f.read()

workflow_old = """section:: Workflow

Create the model only after the server sample rate is known, load it, and wait
for the Buffer before starting the Synth:

code::
(
s.waitForBoot {
    Routine({
        var model = GroupedFDNModel.twoRooms(
            0.5, 4, 0.2, s.sampleRate, 2
        );
        var buffer = model.load(s);
        s.sync;
        {
            var fade = EnvGen.kr(Env([0, 1], [0.08]));
            GroupedFDN.ar(
                Impulse.ar(0.5), buffer, 2, 4
            ) * fade * 0.4
        }.play;
    }).play;
};
)
::"""

workflow_new = """section:: Workflow

Create the model only after the server sample rate is known, load it, and wait for the Buffer before starting the Synth. Evaluating this step-by-step is the safest approach:

code::
// 1. Boot server
s.boot;

// 2. Prepare the model
~model = GroupedFDNModel.twoRooms(0.5, 4, 0.2, s.sampleRate, 2).load(s);

// 3. Play the synth (run this after the model buffer has loaded)
(
x = {
    var fade = EnvGen.kr(Env([0, 1], [0.08]));
    GroupedFDN.ar(Impulse.ar(0.5), ~model, 2, 4)[0] * fade * 0.4
}.play;
)

// 4. Cleanup
x.free; ~model.free;
::"""

plot_old_regex = r"section:: Plotting Impulse Responses in SuperCollider.*?::"
plot_old = re.search(plot_old_regex, content, re.DOTALL).group(0)

plot_new = """section:: Plotting Impulse Responses in SuperCollider

If you wish to visualize the impulse response of any reverb yourself natively within the SuperCollider IDE, you can easily shoot a single impulse through the UGen, record the output to a temporary buffer, and use the `.plot` method.

code::
// 1. Boot server
s.boot;

// 2. Prepare models and allocate a 3-channel buffer for 1 second of audio
(
~rirModel = RIRFDNModel.synthetic(sampleRate: s.sampleRate).load(s);
~vfdnModel = VelvetFDNModel.default(8, 3, s.sampleRate).load(s);
~grpModel = GroupedFDNModel.twoRooms(0.5, 4, 0.2, s.sampleRate, 2).load(s);
~buf = Buffer.alloc(s, s.sampleRate.asInteger, 3);
)

// 3. Record the impulse responses
(
~recSynth = {
    var imp = Impulse.ar(0);
    var sig = [
        RIRFDN.ar(imp, ~rirModel)[0],
        VelvetFDN.ar(imp, ~vfdnModel)[0],
        GroupedFDN.ar(imp, ~grpModel)[0]
    ];
    RecordBuf.ar(sig, ~buf, loop: 0, doneAction: 2);
    Line.kr(0, 1, 1, doneAction: 2); // Free the synth after 1 second
}.play;
)

// 4. Plot them together (run this after recording finishes)
(
~buf.loadToFloatArray(action: { |array|
    defer {
        // Separate the interleaved buffer array into 3 distinct channels
        var channels = array.clump(3).flop;
        var plt = channels.plot("Impulse Responses: RIRFDN (top), VelvetFDN (mid), GroupedFDN (bot)");
        plt.superpose_(false); // Stack them rather than overlaying
    };
});
)

// 5. Cleanup
~buf.free; ~rirModel.free; ~vfdnModel.free; ~grpModel.free;
::"""

content = content.replace(workflow_old, workflow_new)
content = content.replace(plot_old, plot_new)

with open(file_path, 'w') as f:
    f.write(content)

print("Replaced guide examples")
